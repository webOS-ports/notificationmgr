// Copyright (c) 2013-2024 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "History.h"
#include "NotificationService.h"
#include "LSUtils.h"
#include "Utils.h"
#include "JUtil.h"
#include "Logging.h"
#include "SystemTime.h"
#include <string>
#include <pbnjson.hpp>

#define DB8_KIND "com.webos.notificationhistory:1"

#define MAX_TIMESTAMP 253402300799

static History* s_history_instance = 0;

using namespace std::placeholders;

History::History()
    : m_expireData(false)
{
    s_history_instance = this;

    m_connSystemTimeSync = SystemTime::instance().sigSync.connect(
        std::bind(&History::onSystemTimeSync, this, _1)
    );
}

History::~History()
{

}

History* History::instance()
{
	if(!s_history_instance) {
		return new History();
	}

	return s_history_instance;
}

/*
 * db8 queries used to be built by pasting values into a json string with
 * g_strdup_printf or operator+. Everything that reaches here is caller
 * supplied - a sourceId, a notification id, a package name - so a value
 * carrying a quote did not end up as a value at all: it closed the string
 * and the rest of it was read as query syntax. Build the object and let the
 * generator do the escaping.
 */
pbnjson::JValue History::whereClause(const std::string& prop, const pbnjson::JValue& val, const char* op)
{
    pbnjson::JValue clause = pbnjson::Object();
    clause.put("prop", prop);
    clause.put("op", op);
    clause.put("val", val);

    pbnjson::JValue where = pbnjson::Array();
    where.append(clause);
    return where;
}

pbnjson::JValue History::findQuery(const pbnjson::JValue& where)
{
    pbnjson::JValue from = pbnjson::Object();
    from.put("from", DB8_KIND);
    from.put("where", where);

    pbnjson::JValue query = pbnjson::Object();
    query.put("query", from);
    return query;
}

pbnjson::JValue History::purgeQuery(const pbnjson::JValue& where)
{
    pbnjson::JValue query = findQuery(where);
    query.put("purge", true);
    return query;
}

void History::saveMessage(pbnjson::JValue msg)
{
	LSErrorSafe lserror;
	pbnjson::JValue objArray = pbnjson::Array();
	pbnjson::JValue payload = pbnjson::Object();

	pbnjson::JValue scheduleInMsg = msg["schedule"];
        if(scheduleInMsg.isNull())
        {
         pbnjson::JValue schedule = pbnjson::Object();
         schedule.put("expire", MAX_TIMESTAMP);
         msg.put("schedule", schedule);
        }

	//Add kind to the object
	msg.put("_kind", DB8_KIND);
	objArray.put(0, msg);

	payload.put("objects", objArray);
	std::string msgPayload = pbnjson::JGenerator::serialize(payload, pbnjson::JSchemaFragment("{}"));

	if (LSCallOneReply(NotificationService::instance()->getHandle(),"palm://com.palm.db/put",
							msgPayload.c_str(),
							History::cbDb8Response,NULL,NULL, &lserror) == false) {
				 LOG_WARNING(MSGID_SAVE_MSG_FAIL, 0, "Save Message to History table call failed in %s", __PRETTY_FUNCTION__ );
	}

}

void History::deleteMessage(const std::string &key, const std::string& value)
{
    LSErrorSafe lserror;

    std::string query = JUtil::jsonToString(
        purgeQuery(whereClause(key, pbnjson::JValue(value))));

    if (LSCallOneReply(NotificationService::instance()->getHandle(),"palm://com.palm.db/del",
        query.c_str(),
        History::cbDb8Response,NULL,NULL, &lserror) == false)
    {
        LOG_WARNING(MSGID_DEL_MSG_FAIL, 0, "Delete Message from History table call failed in %s", __PRETTY_FUNCTION__ );
    }
}

bool History::selectMessage(LSHandle* lshandle, const std::string& id, LSMessage *message)
{
    LSErrorSafe lserror;

    pbnjson::JValue request;
    JUtil::Error error;

    request = JUtil::parse(LSMessageGetPayload(message), "", &error);

    if(request.isNull())
    {
        return false;
    }

    pbnjson::JValue where;
    if(id == "all")
        where = whereClause("saveRemoteNotification", pbnjson::JValue(false));
    else
        where = whereClause("sourceId", pbnjson::JValue(id));

    std::string query = JUtil::jsonToString(findQuery(where));

    LOG_DEBUG("[%s:%d] query = %s", __FUNCTION__, __LINE__, query.c_str());

    /* The reply travels with the call rather than in a member: two
     * overlapping getNotification calls used to share one slot, so the
     * first was never answered, its reference was leaked, and the second
     * was answered and unreferenced twice.
     */
    LSMessageRef(message);
    if (LSCallOneReply(lshandle, "palm://com.palm.db/find",
                query.c_str(),
                History::cbDb8getNotiResponse, message, NULL, &lserror) == false) {
        LOG_WARNING(MSGID_SAVE_MSG_FAIL, 0, "Select Message to History table call failed in %s", __PRETTY_FUNCTION__ );
        LSMessageUnref(message);
        return false;
    }
    return true;
}

bool History::selectToastMessage(LSHandle* lshandle, const std::string& id, LSMessage *message)
{
    LSErrorSafe lserror;

    pbnjson::JValue request;
    JUtil::Error error;

    request = JUtil::parse(LSMessageGetPayload(message), "", &error);

    if(request.isNull())
    {
        return false;
    }

    int display_id = request["displayId"].asNumber<int>();
    std::string query = JUtil::jsonToString(
        findQuery(whereClause("displayId", pbnjson::JValue(display_id))));

    LSMessageRef(message);
    if (LSCallOneReply(lshandle, "palm://com.palm.db/find",
                       query.c_str(),
                       History::cbDb8getToastResponse, message, NULL, &lserror) == false) {
        LOG_WARNING(MSGID_SAVE_MSG_FAIL, 0, "Select Message to History table call failed in %s", __PRETTY_FUNCTION__ );
        LSMessageUnref(message);
        return false;
    }
    return true;
}

bool History::selectRemoteMessage(LSHandle* lshandle, const std::string& id, LSMessage *message)
{
    LSErrorSafe lserror;

    pbnjson::JValue where;
    if(id == "all")
        where = whereClause("saveRemoteNotification", pbnjson::JValue(true));
    else
        where = whereClause("remotePackageName", pbnjson::JValue(id));

    std::string query = JUtil::jsonToString(findQuery(where));

    LOG_DEBUG("[%s:%d] query = %s", __FUNCTION__, __LINE__, query.c_str());

    LSMessageRef(message);
    if (LSCallOneReply(lshandle, "palm://com.palm.db/find",
                query.c_str(),
                History::cbDb8getRemoteNotiResponse, message, NULL, &lserror) == false) {
        LOG_WARNING(MSGID_SAVE_MSG_FAIL, 0, "Select Message to History table call failed in %s", __PRETTY_FUNCTION__ );
        LSMessageUnref(message);
        return false;
    }

    return true;
}


bool History::deleteNotiMessage(pbnjson::JValue notificationPayload)
{

	return deleteNotiMessageFromDb(NotificationService::instance()->getHandle(), notificationPayload, "removeNotiId", "sourceId", "sourceId","notiId");
	/*
    LSErrorSafe lserror;

    std::string errText;
    std::string removeNotiBySourceId;

    pbnjson::JValue removeNotiIdObj = pbnjson::Object();
    pbnjson::JValue removeSourceIdObj = pbnjson::Object();

    removeNotiIdObj = notificationPayload["removeNotiId"];
    removeSourceIdObj = notificationPayload["sourceId"];
    LOG_WARNING(MSGID_NOTIFICATIONMGR, 0, "[%s:%d]", __FUNCTION__, __LINE__);

    if(!removeSourceIdObj.isNull())
    {
        LOG_WARNING(MSGID_NOTIFICATIONMGR, 0, "[%s:%d]", __FUNCTION__, __LINE__);
        removeNotiBySourceId = removeSourceIdObj.asString();
        if(removeNotiBySourceId.length() == 0)
        {
            LOG_WARNING("sourceId is 0", 0, "No sourceId are given in %s", __PRETTY_FUNCTION__);
        }
        else
        {
            LOG_DEBUG("remove sourceId = %s", removeNotiBySourceId.c_str());
            gchar* query = g_strdup_printf ("{\"query\":{\"from\":\"com.webos.notificationhistory:1\",\"where\":[{\"prop\":\"notiInfo.sourceId\",\"op\":\"=\",\"val\":\"%s\"}]},\"purge\":true}", removeNotiBySourceId.c_str());

            LOG_WARNING(MSGID_NOTIFICATIONMGR, 0, "[%s:%d] query = %s", __FUNCTION__, __LINE__, query);
            if (LSCallOneReply(NotificationService::instance()->getHandle(),"palm://com.palm.db/del",
                            query,
                            History::cbDb8Response,NULL,NULL, &lserror) == false) {
                LOG_WARNING(MSGID_DEL_MSG_FAIL, 0, "Delete Message from History table call failed in %s", __PRETTY_FUNCTION__ );
            }
            g_free (query);
            LOG_WARNING(MSGID_NOTIFICATIONMGR, 0, "[%s:%d]", __FUNCTION__, __LINE__);
        }
    }

    if(removeNotiIdObj.isArray())
    {
        LOG_WARNING(MSGID_NOTIFICATIONMGR, 0, "[%s:%d]", __FUNCTION__, __LINE__);
        if(removeNotiIdObj.arraySize() == 0)
        {
            LOG_WARNING("notiId is 0", 0, "No notiId are given in %s", __PRETTY_FUNCTION__);
        }

        for(ssize_t index = 0; index < removeNotiIdObj.arraySize(); ++index)
        {
            std::string notiId = removeNotiIdObj[index].asString();
            LOG_DEBUG("remove notiId Payload = %s", notiId.c_str());

            gchar* query = g_strdup_printf ("{\"query\":{\"from\":\"com.webos.notificationhistory:1\",\"where\":[{\"prop\":\"notiInfo.notiId\",\"op\":\"=\",\"val\":\"%s\"}]},\"purge\":true}", notiId.c_str());

            LOG_WARNING(MSGID_NOTIFICATIONMGR, 0, "[%s:%d] query = %s", __FUNCTION__, __LINE__, query);
            if (LSCallOneReply(NotificationService::instance()->getHandle(),"palm://com.palm.db/del",
                            query,
                            History::cbDb8Response,NULL,NULL, &lserror) == false) {
                LOG_WARNING(MSGID_DEL_MSG_FAIL, 0, "Delete Message from History table call failed in %s", __PRETTY_FUNCTION__ );
            }
            g_free (query);
            LOG_WARNING(MSGID_NOTIFICATIONMGR, 0, "[%s:%d]", __FUNCTION__, __LINE__);
        }
    }
    */
}

bool History::deleteNotiMessageFromDb(LSHandle* lsHandle, pbnjson::JValue notificationPayload, const std::string& id, const std::string& idName, const std::string& propertyName, const std::string& propertyNameInArray)
{
	LSErrorSafe lserror;
	bool returnValue = true;

    std::string errText;
    std::string removeNotiByName;

    pbnjson::JValue removeNotiIdObj = pbnjson::Object();
    pbnjson::JValue removeNotiNameObj = pbnjson::Object();

    removeNotiIdObj = notificationPayload[id];
    removeNotiNameObj = notificationPayload[idName];
    LOG_WARNING(MSGID_NOTIFICATIONMGR, 0, "[%s:%d]", __FUNCTION__, __LINE__);

    if(removeNotiIdObj.isArray())
    {
        LOG_WARNING(MSGID_NOTIFICATIONMGR, 0, "[%s:%d]", __FUNCTION__, __LINE__);
        if(removeNotiIdObj.arraySize() == 0)
        {
            LOG_WARNING("notiId is 0", 0, "No notiId are given in %s", __PRETTY_FUNCTION__);
        }

        for(ssize_t index = 0; index < removeNotiIdObj.arraySize(); ++index)
        {
            std::string notiId = removeNotiIdObj[index].asString();
            LOG_DEBUG("remove notiId Payload = %s", notiId.c_str());

            std::string query = JUtil::jsonToString(
                purgeQuery(whereClause(propertyNameInArray, pbnjson::JValue(notiId))));

            LOG_DEBUG("[%s:%d] query = %s", __FUNCTION__, __LINE__, query.c_str());
            if (LSCallOneReply(NotificationService::instance()->getHandle(),"palm://com.palm.db/del",
                            query.c_str(),
                            History::cbDb8Response,NULL,NULL, &lserror) == false) {
                returnValue = false;
                LOG_WARNING(MSGID_DEL_MSG_FAIL, 0, "Delete Message from History table call failed in %s", __PRETTY_FUNCTION__ );
            }
        }
    }
    else if(!removeNotiNameObj.isNull())
    {
        LOG_WARNING(MSGID_NOTIFICATIONMGR, 0, "[%s:%d]", __FUNCTION__, __LINE__);
        removeNotiByName = removeNotiNameObj.asString();
        if(removeNotiByName.length() == 0)
        {
            LOG_WARNING("sourceId is 0", 0, "No sourceId are given in %s", __PRETTY_FUNCTION__);
        }
        else
        {
            LOG_DEBUG("remove notification = %s", removeNotiByName.c_str());
            std::string query = JUtil::jsonToString(
                purgeQuery(whereClause(propertyName, pbnjson::JValue(removeNotiByName))));

            LOG_DEBUG("[%s:%d] query = %s", __FUNCTION__, __LINE__, query.c_str());
            if (LSCallOneReply(lsHandle,"palm://com.palm.db/del",
                            query.c_str(),
                            History::cbDb8Response,NULL,NULL, &lserror) == false) {
            	returnValue = false;
                LOG_WARNING(MSGID_DEL_MSG_FAIL, 0, "Delete Message from History table call failed in %s", __PRETTY_FUNCTION__ );
            }
        }
    }


    return returnValue;

}

bool History::deleteRemoteNotiMessage(LSHandle* lsHandle, pbnjson::JValue notificationPayload)
{

	return deleteNotiMessageFromDb(lsHandle, notificationPayload, "removeRemoteNotiId", "remotePackageName", "remotePackageName","remoteNotiId");
	/*
    LSErrorSafe lserror;

    std::string errText;
    std::string removeRemoteNotiByPkgName;

    pbnjson::JValue removeRemoteNotiIdObj = pbnjson::Object();
    pbnjson::JValue removeRemotePkgNameObj = pbnjson::Object();

    removeRemoteNotiIdObj = notificationPayload["removeRemoteNotiId"];
    removeRemotePkgNameObj = notificationPayload["remotePackageName"];

    LOG_WARNING(MSGID_NOTIFICATIONMGR, 0, "[%s:%d]", __FUNCTION__, __LINE__);
    if(!removeRemotePkgNameObj.isNull())
    {
        LOG_WARNING(MSGID_NOTIFICATIONMGR, 0, "[%s:%d]", __FUNCTION__, __LINE__);
        removeRemoteNotiByPkgName = removeRemotePkgNameObj.asString();
        if(removeRemoteNotiByPkgName.length() == 0)
        {
            LOG_WARNING("sourceId is 0", 0, "No sourceId are given in %s", __PRETTY_FUNCTION__);
        }
        else
        {
            LOG_DEBUG("removeRemoteNotiByPkgName = %s", removeRemoteNotiByPkgName.c_str());
            gchar* query = g_strdup_printf ("{\"query\":{\"from\":\"com.webos.notificationhistory:1\",\"where\":[{\"prop\":\"notiInfo.remotePackageName\",\"op\":\"=\",\"val\":\"%s\"}]},\"purge\":true}", removeRemoteNotiByPkgName.c_str());

            LOG_WARNING(MSGID_NOTIFICATIONMGR, 0, "[%s:%d] query = %s", __FUNCTION__, __LINE__, query);
            if (LSCallOneReply(lsHandle, "palm://com.palm.db/del",
                            query,
                            History::cbDb8Response,NULL,NULL, &lserror) == false) {
                LOG_WARNING(MSGID_DEL_MSG_FAIL, 0, "Delete Message from History table call failed in %s", __PRETTY_FUNCTION__ );
            }
            g_free (query);
            LOG_WARNING(MSGID_NOTIFICATIONMGR, 0, "[%s:%d]", __FUNCTION__, __LINE__);
        }
    }

    if(removeRemoteNotiIdObj.isArray())
    {
        LOG_WARNING(MSGID_NOTIFICATIONMGR, 0, "[%s:%d]", __FUNCTION__, __LINE__);
        if(removeRemoteNotiIdObj.arraySize() == 0)
        {
            LOG_WARNING("notiId is 0", 0, "No remoteNotiId are given in %s", __PRETTY_FUNCTION__);
        }

        for(ssize_t index = 0; index < removeRemoteNotiIdObj.arraySize(); ++index)
        {
            std::string remoteNotiId = removeRemoteNotiIdObj[index].asString();
            LOG_DEBUG("remove remoteNotiId Payload = %s", remoteNotiId.c_str());

            gchar* query = g_strdup_printf ("{\"query\":{\"from\":\"com.webos.notificationhistory:1\",\"where\":[{\"prop\":\"notiInfo.remoteNotiId\",\"op\":\"=\",\"val\":\"%s\"}]},\"purge\":true}", remoteNotiId.c_str());

            LOG_WARNING(MSGID_NOTIFICATIONMGR, 0, "[%s:%d] query = %s", __FUNCTION__, __LINE__, query);
            if (LSCallOneReply(lsHandle, "palm://com.palm.db/del",
                            query,
                            History::cbDb8Response,NULL,NULL, &lserror) == false) {
                LOG_WARNING(MSGID_DEL_MSG_FAIL, 0, "Delete Message from History table call failed in %s", __PRETTY_FUNCTION__ );
            }
            g_free (query);
            LOG_WARNING(MSGID_NOTIFICATIONMGR, 0, "[%s:%d]", __FUNCTION__, __LINE__);
        }
    }

    return true;
    */
}

bool History::cbDb8Response(LSHandle* lshandle, LSMessage *message, void *user_data)
{
    LSErrorSafe lserror;

    pbnjson::JValue request;
    JUtil::Error error;

    request = JUtil::parse(LSMessageGetPayload(message), "", &error);

    if(request.isNull())
    {
        LOG_WARNING(MSGID_DB8_NULL_RESP, 0, "Db8 LS2 response is empty in %s", __PRETTY_FUNCTION__ );
        return false;
    }

    if(!request["returnValue"].asBool())
    {
        LOG_WARNING(MSGID_DB8_CALL_FAILED, 0, "Call to Db8 to save/delete message failed in %s", __PRETTY_FUNCTION__ );
        return false;
    }

    LOG_DEBUG("[DB8Response] result:%s", LSMessageGetPayload(message));

    return true;
}

bool History::cbDb8getNotiResponse(LSHandle* lshandle, LSMessage *message, void *user_data)
{
    LSErrorSafe lserror;
    std::string errText;

    pbnjson::JValue request;
    pbnjson::JValue resultArray;
    pbnjson::JValue notiInfoArray = pbnjson::Array();

    /* The message this reply belongs to travelled with the call. */
    LSMessage* getNotiReplyMsg = static_cast<LSMessage*>(user_data);
    if (!getNotiReplyMsg)
        return false;

    JUtil::Error error;

    bool success = false;

    request = JUtil::parse(LSMessageGetPayload(message), "", &error);

    LOG_WARNING(MSGID_NOTIFICATIONMGR, 0, "[%s:%d]", __FUNCTION__, __LINE__);
    if(request.isNull())
    {
        LOG_WARNING(MSGID_DB8_NULL_RESP, 0, "Db8 LS2 response is empty in %s", __PRETTY_FUNCTION__ );
        errText = "Message is not parsed";
        goto Done;
    }

    if(!request["returnValue"].asBool())
    {
        LOG_WARNING(MSGID_DB8_CALL_FAILED, 0, "Call to Db8 to get message failed in %s", __PRETTY_FUNCTION__ );
        errText = "Call to Db8 to get message failed";
        goto Done;
    }

    resultArray = request["results"];

    if(resultArray.isArray())
    {
        if(resultArray.arraySize() == 0)
        {
            LOG_DEBUG("DB result is 0 %s", __PRETTY_FUNCTION__);
        }

        for(ssize_t index = 0; index < resultArray.arraySize() ; ++index) {
            pbnjson::JValue notiInfoObj = pbnjson::Object();
            if(!resultArray[index]["sourceId"].isNull())
            {
                notiInfoObj.put("sourceId", resultArray[index]["sourceId"]);
            }
            if(!resultArray[index]["notiId"].isNull())
            {
                notiInfoObj.put("notiId", resultArray[index]["notiId"]);
            }
            if(!resultArray[index]["timestamp"].isNull())
            {
                notiInfoObj.put("timestamp", resultArray[index]["timestamp"]);
            }
            if(!resultArray[index]["iconUrl"].isNull())
            {
                notiInfoObj.put("iconUrl", resultArray[index]["iconUrl"]);
            }
            if(!resultArray[index]["title"].isNull())
            {
                notiInfoObj.put("title", resultArray[index]["title"]);
            }
            if(!resultArray[index]["message"].isNull())
            {
                notiInfoObj.put("message", resultArray[index]["message"]);
            }
            if(!resultArray[index]["autoRemove"].isNull())
            {
                notiInfoObj.put("autoRemove", resultArray[index]["autoRemove"]);
            }
            if(!resultArray[index]["onClick"].isNull())
            {
                notiInfoObj.put("onClick", resultArray[index]["onClick"]);
            }
            if(!resultArray[index]["params"].isNull())
            {
                notiInfoObj.put("params", resultArray[index]["params"]);
            }
            if(!resultArray[index]["forceLcdTurnOn"].isNull())
            {
                notiInfoObj.put("forceLcdTurnOn", resultArray[index]["forceLcdTurnOn"]);
            }
            if(!resultArray[index]["needSoundPlay"].isNull())
            {
                notiInfoObj.put("needSoundPlay", resultArray[index]["needSoundPlay"]);
            }
            if(!resultArray[index]["forceSoundPlay"].isNull())
            {
                notiInfoObj.put("forceSoundPlay", resultArray[index]["forceSoundPlay"]);
            }
            if(!resultArray[index]["soundUri"].isNull())
            {
                notiInfoObj.put("soundUri", resultArray[index]["soundUri"]);
            }
            if(!resultArray[index]["isRawSound"].isNull())
            {
                notiInfoObj.put("isRawSound", resultArray[index]["isRawSound"]);
            }
            if(!resultArray[index]["needToShowPopup"].isNull())
            {
                notiInfoObj.put("needToShowPopup", resultArray[index]["needToShowPopup"]);
            }
            if(!resultArray[index]["isRemoteNotification"].isNull())
            {
                notiInfoObj.put("isRemoteNotification", resultArray[index]["isRemoteNotification"]);
            }
            if(!resultArray[index]["isUnDeletable"].isNull())
            {
                notiInfoObj.put("isUnDeletable", resultArray[index]["isUnDeletable"]);
            }
            if(!resultArray[index]["isSysReq"].isNull())
            {
                notiInfoObj.put("isSysReq", resultArray[index]["isSysReq"]);
            }
            if(!resultArray[index]["saveRemoteNotification"].isNull())
            {
                notiInfoObj.put("saveRemoteNotification", resultArray[index]["saveRemoteNotification"]);
            }
            notiInfoArray.put(index, notiInfoObj);
        }
    }

    success = true;

Done:
    pbnjson::JValue json = pbnjson::Object();
    json.put("returnValue", success);
    json.put("notiInfo", notiInfoArray);
    json.put("count", notiInfoArray.arraySize());

    if(!success)
    {
        json.put("errorText", errText);
    }

    {
        std::string result = JUtil::jsonToString(std::move(json));
        LOG_DEBUG("==== cbDb8getNotiResponse Payload ==== %s", result.c_str());

        bool replied = LSMessageReply( lshandle, getNotiReplyMsg, result.c_str(), &lserror);
        LSMessageUnref(getNotiReplyMsg);
        return replied;
    }
}

bool History::cbDb8getToastResponse(LSHandle* lshandle, LSMessage *message, void *user_data)
{
    LSErrorSafe lserror;
    std::string errText;

    pbnjson::JValue request;
    pbnjson::JValue resultArray;
    pbnjson::JValue toastInfoArray = pbnjson::Array();

    /* The message this reply belongs to travelled with the call. */
    LSMessage* getToastReplyMsg = static_cast<LSMessage*>(user_data);
    if (!getToastReplyMsg)
        return false;

    JUtil::Error error;

    bool success = false;

    request = JUtil::parse(LSMessageGetPayload(message), "", &error);
    std::string testJson = JUtil::jsonToString(request);

    if(request.isNull())
    {
        LOG_WARNING(MSGID_DB8_NULL_RESP, 0, "Db8 LS2 response is empty in %s", __PRETTY_FUNCTION__ );
        errText = "Message is not parsed";
        goto Done;
    }

    if(!request["returnValue"].asBool())
    {
        LOG_WARNING(MSGID_DB8_CALL_FAILED, 0, "Call to Db8 to get message failed in %s", __PRETTY_FUNCTION__ );
        errText = "Call to Db8 to get message failed";
        goto Done;
    }

    resultArray = request["results"];

    if(resultArray.isArray())
    {
        if(resultArray.arraySize() == 0)
        {
            LOG_DEBUG("DB result is 0 %s", __PRETTY_FUNCTION__);
        }

        for(ssize_t index = 0; index < resultArray.arraySize() ; ++index) {
            pbnjson::JValue toastInfoObj = pbnjson::Object();
            if(!resultArray[index]["sourceId"].isNull())
            {
                toastInfoObj.put("sourceId", resultArray[index]["sourceId"]);
            }
            if(!resultArray[index]["toastId"].isNull())
            {
                toastInfoObj.put("toastId", resultArray[index]["notiId"]);
            }
			else
            {
                std::string sourceId = resultArray[index]["sourceId"].asString();
                std::string timestamp = resultArray[index]["timestamp"].asString();
                toastInfoObj.put("toastId", (sourceId + "-" + timestamp));
            }
            if(!resultArray[index]["timestamp"].isNull())
            {
                toastInfoObj.put("timestamp", resultArray[index]["timestamp"]);
            }
            if(!resultArray[index]["iconUrl"].isNull())
            {
                toastInfoObj.put("iconUrl", resultArray[index]["iconUrl"]);
            }
            if(!resultArray[index]["iconPath"].isNull())
            {
                toastInfoObj.put("iconPath", resultArray[index]["iconPath"]);
            }
            if(!resultArray[index]["title"].isNull())
            {
                toastInfoObj.put("title", resultArray[index]["title"]);
            }
            if(!resultArray[index]["message"].isNull())
            {
                toastInfoObj.put("message", resultArray[index]["message"]);
            }
            if(!resultArray[index]["isSysReq"].isNull())
            {
                toastInfoObj.put("isSysReq", resultArray[index]["isSysReq"]);
            }
            if(!resultArray[index]["displayId"].isNull())
            {
                toastInfoObj.put("displayId", resultArray[index]["displayId"]);
            }
            if(!resultArray[index]["user"].isNull())
            {
                toastInfoObj.put("user", resultArray[index]["user"]);
            }
            if(!resultArray[index]["schedule"].isNull())
            {
                toastInfoObj.put("schedule", resultArray[index]["schedule"]);
            }
            if(!resultArray[index]["type"].isNull())
            {
                toastInfoObj.put("type", resultArray[index]["type"]);
            }
            if(!resultArray[index]["action"].isNull())
            {
                toastInfoObj.put("action", resultArray[index]["action"]);
            }
            if(!resultArray[index]["readStatus"].isNull())
            {
                toastInfoObj.put("readStatus", resultArray[index]["readStatus"]);
            }
            toastInfoArray.put(index, toastInfoObj);
        }
    }

    success = true;

Done:
    pbnjson::JValue json = pbnjson::Object();
    json.put("returnValue", success);
    json.put("toastInfo", toastInfoArray);
    json.put("count", toastInfoArray.arraySize());

    if(!success)
    {
        json.put("errorText", errText);
    }

    {
        std::string result = JUtil::jsonToString( std::move(json));
        LOG_DEBUG("==== cbDb8getToastResponse Payload ==== %s", result.c_str());

        bool replied = LSMessageReply( lshandle, getToastReplyMsg, result.c_str(), &lserror);
        LSMessageUnref(getToastReplyMsg);
        return replied;
    }
}

bool History::cbDb8getRemoteNotiResponse(LSHandle* lshandle, LSMessage *message, void *user_data)
{
    LSErrorSafe lserror;
    std::string errText;

    pbnjson::JValue request;
    pbnjson::JValue resultArray;
    pbnjson::JValue remoteNotiInfoArray = pbnjson::Array();

    /* The message this reply belongs to travelled with the call. */
    LSMessage* getNotiReplyMsg = static_cast<LSMessage*>(user_data);
    if (!getNotiReplyMsg)
        return false;

    JUtil::Error error;

    bool success = false;
    request = JUtil::parse(LSMessageGetPayload(message), "", &error);

    LOG_WARNING(MSGID_NOTIFICATIONMGR, 0, "[%s:%d]", __FUNCTION__, __LINE__);
    if(request.isNull())
    {
        LOG_WARNING(MSGID_DB8_NULL_RESP, 0, "Db8 LS2 response is empty in %s", __PRETTY_FUNCTION__ );
        errText = "Message is not parsed";
        goto Done;
    }

    if(!request["returnValue"].asBool())
    {
        LOG_WARNING(MSGID_DB8_CALL_FAILED, 0, "Call to Db8 to get message failed in %s", __PRETTY_FUNCTION__ );
        errText = "Call to Db8 to get message failed";
        goto Done;
    }

    resultArray = request["results"];

    if(resultArray.isArray())
    {
        if(resultArray.arraySize() == 0)
        {
            LOG_DEBUG("DB result is 0 %s", __PRETTY_FUNCTION__);
        }

        for(ssize_t index = 0; index < resultArray.arraySize() ; ++index) {
            pbnjson::JValue remoteNotiInfoObj = pbnjson::Object();

            if(!resultArray[index]["remoteSourceId"].isNull())
            {
                remoteNotiInfoObj.put("remoteSourceId", resultArray[index]["remoteSourceId"]);
            }
            if(!resultArray[index]["remoteNotiId"].isNull())
            {
                remoteNotiInfoObj.put("remoteNotiId", resultArray[index]["remoteNotiId"]);
            }
            if(!resultArray[index]["parentNotiId"].isNull())
            {
                remoteNotiInfoObj.put("parentNotiId", resultArray[index]["parentNotiId"]);
            }
            if(!resultArray[index]["remotePackageName"].isNull())
            {
                remoteNotiInfoObj.put("remotePackageName", resultArray[index]["remotePackageName"]);
            }
            if(!resultArray[index]["remoteTitle"].isNull())
            {
                remoteNotiInfoObj.put("remoteTitle", resultArray[index]["remoteTitle"]);
            }
            if(!resultArray[index]["remoteMessage"].isNull())
            {
                remoteNotiInfoObj.put("remoteMessage", resultArray[index]["remoteMessage"]);
            }
            if(!resultArray[index]["remoteTickerText"].isNull())
            {
                remoteNotiInfoObj.put("remoteTickerText", resultArray[index]["remoteTickerText"]);
            }
            if(!resultArray[index]["remoteId"].isNull())
            {
                remoteNotiInfoObj.put("remoteId", resultArray[index]["remoteId"]);
            }
            if(!resultArray[index]["remoteUserId"].isNull())
            {
                remoteNotiInfoObj.put("remoteUserId", resultArray[index]["remoteUserId"]);
            }
            if(!resultArray[index]["remotePostTime"].isNull())
            {
                remoteNotiInfoObj.put("remotePostTime", resultArray[index]["remotePostTime"]);
            }
            if(!resultArray[index]["remoteAppName"].isNull())
            {
                remoteNotiInfoObj.put("remoteAppName", resultArray[index]["remoteAppName"]);
            }
            if(!resultArray[index]["remoteIconUrl"].isNull())
            {
                remoteNotiInfoObj.put("remoteIconUrl", resultArray[index]["remoteIconUrl"]);
            }
            if(!resultArray[index]["remoteBgImage"].isNull())
            {
                remoteNotiInfoObj.put("remoteBgImage", resultArray[index]["remoteBgImage"]);
            }
            if(!resultArray[index]["remoteActionParam"].isNull())
            {
                remoteNotiInfoObj.put("remoteActionParam", resultArray[index]["remoteActionParam"]);
            }
            if(!resultArray[index]["timestamp"].isNull())
            {
                remoteNotiInfoObj.put("timestamp", resultArray[index]["timestamp"]);
            }
            if(!resultArray[index]["isUnDeletable"].isNull())
            {
                remoteNotiInfoObj.put("isUnDeletable", resultArray[index]["isUnDeletable"]);
            }
            if(!resultArray[index]["isRemoteNotification"].isNull())
            {
                remoteNotiInfoObj.put("isRemoteNotification", resultArray[index]["isRemoteNotification"]);
            }
            if(!resultArray[index]["saveRemoteNotification"].isNull())
            {
                remoteNotiInfoObj.put("saveRemoteNotification", resultArray[index]["saveRemoteNotification"]);
            }
            if(!resultArray[index]["groupId"].isNull())
            {
                remoteNotiInfoObj.put("groupId", resultArray[index]["groupId"]);
            }
            if(!resultArray[index]["notificationType"].isNull())
            {
                remoteNotiInfoObj.put("notificationType", resultArray[index]["notificationType"]);
            }
            if(!resultArray[index]["pageNumber"].isNull())
            {
                remoteNotiInfoObj.put("pageNumber", resultArray[index]["pageNumber"]);
            }
            if(!resultArray[index]["remoteSubText"].isNull())
            {
                remoteNotiInfoObj.put("remoteSubText", resultArray[index]["remoteSubText"]);
            }
            if(!resultArray[index]["remoteCount"].isNull())
            {
                remoteNotiInfoObj.put("remoteCount", resultArray[index]["remoteCount"]);
            }
            if(!resultArray[index]["remoteTag"].isNull())
            {
                remoteNotiInfoObj.put("remoteTag", resultArray[index]["remoteTag"]);
            }
            if(!resultArray[index]["remoteResultKey"].isNull())
            {
                remoteNotiInfoObj.put("remoteResultKey", resultArray[index]["remoteResultKey"]);
            }
            if(!resultArray[index]["remoteAlert"].isNull())
            {
                remoteNotiInfoObj.put("remoteAlert", resultArray[index]["remoteAlert"]);
            }
            if(!resultArray[index]["enhancedNotification"].isNull())
            {
                remoteNotiInfoObj.put("enhancedNotification", resultArray[index]["enhancedNotification"]);
            }

            remoteNotiInfoArray.put(index, remoteNotiInfoObj);
        }
    }

    success = true;

Done:
    pbnjson::JValue json = pbnjson::Object();
    json.put("returnValue", success);
    json.put("notiInfo", remoteNotiInfoArray);
    json.put("count", remoteNotiInfoArray.arraySize());

    if(!success)
    {
        json.put("errorText", errText);
    }

    {
        std::string result = JUtil::jsonToString(std::move(json));
        LOG_DEBUG("==== cbDb8getRemoteNotiResponse Payload ==== %s", result.c_str());

        bool replied = LSMessageReply( lshandle, getNotiReplyMsg, result.c_str(), &lserror);
        LSMessageUnref(getNotiReplyMsg);
        return replied;
    }
}

bool History::purgeAllData()
{
    LSErrorSafe lserror;

    static const char* const kEndOfTime = "9999999999";

    pbnjson::JValue where = whereClause("isUnDeletable", pbnjson::JValue(false));
    pbnjson::JValue olderThan = pbnjson::Object();
    olderThan.put("prop", "timestamp");
    olderThan.put("op", "<");
    olderThan.put("val", kEndOfTime);
    where.append(olderThan);

    std::string query = JUtil::jsonToString(purgeQuery(where));

    if (LSCallOneReply(NotificationService::instance()->getHandle(),"palm://com.palm.db/del",
                            query.c_str(),
                            History::cbDb8Response,NULL,NULL, &lserror) == false) {
                LOG_WARNING(MSGID_PURGE_FAIL, 0,"PurgeAllData Db8 LS2 call failed in %s", __PRETTY_FUNCTION__ );
    }

    return true;
}

bool History::resetUserNotifications(int displayId)
{
    LSErrorSafe lserror;

    pbnjson::JValue remove_query = pbnjson::Object();
    pbnjson::JValue request;

    request = pbnjson::JObject{{"from", DB8_KIND},
                               {"where", pbnjson::JArray{{{"prop", "displayId"}, {"op", "="}, {"val", displayId}}}}};
    remove_query.put("query", request);

    if (LSCallOneReply(NotificationService::instance()->getHandle(),"palm://com.palm.db/del",
                       JUtil::jsonToString( std::move(remove_query)).c_str(),
                       History::cbDb8Response,NULL,NULL, &lserror) == false) {
            LOG_WARNING(MSGID_PURGE_FAIL, 0,"PurgeAllData Db8 LS2 call failed in %s", __PRETTY_FUNCTION__ );
    }

    return true;
}

bool History::setReadStatus(std::string toastId, bool readStatus)
{
    LSErrorSafe lserror;
    pbnjson::JValue request;
    pbnjson::JValue statusObj = pbnjson::Object();
    pbnjson::JValue merge_query = pbnjson::Object();

    /* A toastId is sourceId + "-" + timestamp and sourceIds contain
     * dashes of their own - "com.webos.app.notification-11458" is an
     * ordinary one - so the timestamp is what follows the last dash, not
     * the first. Taking the first split "com.webos.app.notification-11458
     * -1757500000000" after "com", and the merge then matched no row at
     * all, or the wrong one. This is the same rule createToast builds the
     * id with and the one Utils::extractTimestampFromId already applies.
     */
    std::string timestamp = Utils::extractTimestampFromId(toastId);
    if (timestamp.empty())
    {
        LOG_WARNING(MSGID_SAVE_MSG_FAIL, 0, "Cannot read a timestamp out of the toastId in %s", __PRETTY_FUNCTION__);
        return false;
    }

    statusObj.put("readStatus", readStatus);
    request = pbnjson::JObject{
                {"from", DB8_KIND},
                {"where", pbnjson::JArray{{{"prop", "timestamp"}, {"op", "="}, {"val", timestamp}}}}};
    merge_query.put("query", request);
    merge_query.put("props", statusObj);

    std::string msgPayload = pbnjson::JGenerator::serialize(merge_query, pbnjson::JSchemaFragment("{}"));

    LOG_DEBUG("[setReadStatus] query: %s", msgPayload.c_str());

    if (LSCallOneReply(NotificationService::instance()->getHandle(),"luna://com.webos.service.db/merge",
                            msgPayload.c_str(),
                            History::cbDb8Response,NULL,NULL, &lserror) == false) {
                 LOG_WARNING(MSGID_SAVE_MSG_FAIL, 0, "Set Status to History table call failed in %s", __PRETTY_FUNCTION__ );
                 return false;
    }
    return true;
}

bool History::purgeExpireData()
{
    time_t currTime = time(NULL);
    if (currTime == -1)
    {
        LOG_WARNING(MSGID_EXPIRE_FAIL, 1,
            PMLOGKS("REASON", "time API failed"),
            " ");
        return false;
    }

    std::string query = JUtil::jsonToString(purgeQuery(
        whereClause("schedule.expire",
                    pbnjson::JValue(static_cast<int64_t>(currTime)), "<")));

    LOG_DEBUG("[purgeExpireData] query:%s", query.c_str());

    LSErrorSafe lserror;
    if (LSCallOneReply(NotificationService::instance()->getHandle(),"palm://com.palm.db/del",
        query.c_str(),
        History::cbDb8Response,NULL,NULL,&lserror) == false)
    {
        LOG_WARNING(MSGID_EXPIRE_FAIL, 1,
            PMLOGKS("REASON", lserror.message ? lserror.message : "unknown"),
            " ");
    }

    return true;
}

void History::onSystemTimeSync(bool sync)
{
    if (sync)
    {
        if (!m_expireData)
        {
            m_expireData = true;
            purgeExpireData();
        }
    }
}

void History::onBoot(const std::string &boot)
{
    if (boot == "warm")
    {
        if (SystemTime::instance().isSynced())
        {
            m_expireData = true;
            purgeExpireData();
        }
    }
}
