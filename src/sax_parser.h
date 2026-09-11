// Copyright (c) 2014-2018 LG Electronics, Inc.
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

#ifndef __LIBXMLPP_MYPARSER_H
#define __LIBXMLPP_MYPARSER_H

#include <libxml++/libxml++.h>
#include <glibmm/ustring.h>
#include <pbnjson.hpp>

class MySaxParser : public xmlpp::SaxParser
{
public:
  MySaxParser();
  virtual ~MySaxParser();
  static int level; //To know the order of parsing

protected:
  /* libxml++-5 hands these an xmlpp::ustring, which is a std::string - not
   * the Glib::ustring the 2.6 API used. Declaring them with the old
   * parameter type does not override anything, it adds an overload that
   * hides the base method, so none of this was ever called. "override"
   * makes the compiler say so if the signature drifts again.
   */
  void on_start_document() override;
  void on_end_document() override;
  void on_start_element(const xmlpp::ustring& name,
                        const AttributeList& properties) override;
  void on_end_element(const xmlpp::ustring& name) override;
  void on_characters(const xmlpp::ustring& characters) override;
  void on_comment(const xmlpp::ustring& text) override;
  void on_warning(const xmlpp::ustring& text) override;
  void on_error(const xmlpp::ustring& text) override;
  void on_fatal_error(const xmlpp::ustring& text) override;
};

class Schedule
{
public:
    Schedule();
    ~Schedule();
    static Schedule *instance();
    void process_Schedule_Objects(pbnjson::JValue &Obj);
    void process_Schedule_OnCharacter(const std::string& key, const std::string& value);
    pbnjson::JValue Period;
    pbnjson::JValue sched;
    std::string CanvasName;
    std::string CanvasPath;
    std::string CanvasType;
    int Zorder;
    static bool schedule_parsing;
};

class Canvas
{
public:
    Canvas();
    ~Canvas();
    static Canvas *instance();
    void process_Canvas_Objects(pbnjson::JValue &Obj, int level);
    void process_Canvas_OnCharacter(const std::string& key, const std::string& value);
    pbnjson::JValue canvas;
    pbnjson::JValue wind_Region;
    pbnjson::JValue content;
    pbnjson::JValue text_Region;
    std::string hAlign;
    std::string vAlign;
    int level;
    int lineSpacing;
    std::string win_bckGrndclr;
    std::string message;
    std::string repeat;
    std::string text_bckGrndclr;
    std::string font;
    bool bold;
    std::string text_color;
    bool italic;
    int text_size;
    bool underline;
    int speed;
    int space;
    std::string line_color;
    int line_thickness;
    bool window_flag;
    bool text_flag;
    bool line_flag;
    bool line_view;
    std::string effect;
    static bool canvas_parsing;
};

#endif //__LIBXMLPP_MYPARSER_H
