/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include <cctype>
#include <cstdio>
#include <cstdint>
#include <limits>

#include "common/lang/comparator.h"
#include "common/log/log.h"
#include "common/type/date_type.h"
#include "common/value.h"
#include "storage/common/column.h"

int DateType::compare(const Value &left, const Value &right) const
{
  ASSERT(left.attr_type() == AttrType::DATES, "left type is not date");
  ASSERT(right.attr_type() == AttrType::DATES, "right type is not date");
  return common::compare_int((void *)&left.value_.int_value_, (void *)&right.value_.int_value_);
}

int DateType::compare(const Column &left, const Column &right, int left_idx, int right_idx) const
{
  ASSERT(left.attr_type() == AttrType::DATES, "left type is not date");
  ASSERT(right.attr_type() == AttrType::DATES, "right type is not date");
  return common::compare_int((void *)&((int *)left.data())[left_idx], (void *)&((int *)right.data())[right_idx]);
}

RC DateType::cast_to(const Value &val, AttrType type, Value &result) const
{
  switch (type) {
    case AttrType::DATES: {
      result.set_date(val.value_.int_value_);
      return RC::SUCCESS;
    }
    case AttrType::CHARS: {
      string str;
      RC     rc = to_string(val, str);
      if (rc != RC::SUCCESS) {
        return rc;
      }
      result.set_string(str.c_str());
      return RC::SUCCESS;
    }
    default: {
      LOG_WARN("unsupported cast from date to type %d", type);
      return RC::SCHEMA_FIELD_TYPE_MISMATCH;
    }
  }
}

RC DateType::set_value_from_str(Value &val, const string &data) const
{
  int32_t date_value = 0;
  RC      rc         = parse_date(data, date_value);
  if (rc != RC::SUCCESS) {
    return rc;
  }
  val.set_date(date_value);
  return RC::SUCCESS;
}

RC DateType::to_string(const Value &val, string &result) const
{
  int date_value = val.value_.int_value_;
  int year       = date_value / 10000;
  int month      = (date_value % 10000) / 100;
  int day        = date_value % 100;

  char buf[16] = {0};
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
  result.assign(buf);
  return RC::SUCCESS;
}

RC DateType::parse_date(const string &data, int32_t &date_value)
{
  if (data.empty()) {
    return RC::SCHEMA_FIELD_TYPE_MISMATCH;
  }

  int values[3]     = {0, 0, 0};
  int digit_cnts[3] = {0, 0, 0};

  size_t pos = 0;
  for (int i = 0; i < 3; i++) {
    if (pos >= data.size()) {
      return RC::SCHEMA_FIELD_TYPE_MISMATCH;
    }

    int value     = 0;
    int digit_cnt = 0;
    while (pos < data.size() && std::isdigit(static_cast<unsigned char>(data[pos]))) {
      int digit = data[pos] - '0';
      if (value > (std::numeric_limits<int>::max() - digit) / 10) {
        return RC::SCHEMA_FIELD_TYPE_MISMATCH;
      }
      value = value * 10 + digit;
      pos++;
      digit_cnt++;
    }

    if (digit_cnt == 0) {
      return RC::SCHEMA_FIELD_TYPE_MISMATCH;
    }

    values[i]     = value;
    digit_cnts[i] = digit_cnt;

    if (i < 2) {
      if (pos >= data.size() || data[pos] != '-') {
        return RC::SCHEMA_FIELD_TYPE_MISMATCH;
      }
      pos++;
    }
  }

  if (pos != data.size()) {
    return RC::SCHEMA_FIELD_TYPE_MISMATCH;
  }

  if (digit_cnts[0] != 4 || digit_cnts[1] > 2 || digit_cnts[2] > 2) {
    return RC::SCHEMA_FIELD_TYPE_MISMATCH;
  }

  const int year  = values[0];
  const int month = values[1];
  const int day   = values[2];
  if (!check_date(year, month, day)) {
    return RC::SCHEMA_FIELD_TYPE_MISMATCH;
  }

  date_value = year * 10000 + month * 100 + day;
  return RC::SUCCESS;
}

bool DateType::check_date(int year, int month, int day)
{
  if (year <= 0 || month <= 0 || month > 12 || day <= 0) {
    return false;
  }

  static const int max_days[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int              month_days    = max_days[month];
  if (month == 2) {
    const bool leap_year = (year % 400 == 0) || (year % 100 != 0 && year % 4 == 0);
    if (leap_year) {
      month_days++;
    }
  }

  return day <= month_days;
}
