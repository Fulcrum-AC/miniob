/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by Codex on 2026/5/1.
//

#include "sql/operator/update_physical_operator.h"

#include <cstring>

#include "common/log/log.h"
#include "sql/expr/tuple.h"
#include "storage/field/field_meta.h"
#include "storage/table/table.h"
#include "storage/trx/trx.h"

UpdatePhysicalOperator::UpdatePhysicalOperator(Table *table, const FieldMeta *field_meta, const Value &value)
    : table_(table), field_meta_(field_meta), value_(value)
{}

RC UpdatePhysicalOperator::open(Trx *trx)
{
  trx_ = trx;

  Value cast_value = value_;
  if (cast_value.attr_type() != field_meta_->type()) {
    Value tmp_value;
    RC    rc = Value::cast_to(cast_value, field_meta_->type(), tmp_value);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to cast update value. rc=%s", strrc(rc));
      return rc;
    }
    cast_value = tmp_value;
  }

  if (children_.empty()) {
    LOG_WARN("update physical operator has no child");
    return RC::INTERNAL;
  }

  RC                           rc    = RC::SUCCESS;
  unique_ptr<PhysicalOperator> &child = children_[0];
  rc = child->open(trx);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to open child operator. rc=%s", strrc(rc));
    return rc;
  }

  while (OB_SUCC(rc = child->next())) {
    Tuple *tuple = child->current_tuple();
    if (nullptr == tuple) {
      LOG_WARN("failed to get current tuple while updating");
      child->close();
      return RC::INTERNAL;
    }

    RowTuple *row_tuple = static_cast<RowTuple *>(tuple);
    Record   &record    = row_tuple->record();

    Record old_record;
    rc = old_record.copy_data(record.data(), record.len());
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to copy old record data. rc=%s", strrc(rc));
      child->close();
      return rc;
    }
    old_record.set_rid(record.rid());

    Record new_record;
    rc = new_record.copy_data(record.data(), record.len());
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to copy new record data. rc=%s", strrc(rc));
      child->close();
      return rc;
    }
    new_record.set_rid(record.rid());

    rc = apply_value_to_record(cast_value, new_record);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to apply value to record. rc=%s", strrc(rc));
      child->close();
      return rc;
    }

    old_records_.emplace_back(std::move(old_record));
    new_records_.emplace_back(std::move(new_record));
  }

  RC close_child_rc = child->close();
  if (close_child_rc != RC::SUCCESS) {
    LOG_WARN("failed to close child operator. rc=%s", strrc(close_child_rc));
    return close_child_rc;
  }

  if (rc != RC::RECORD_EOF) {
    LOG_WARN("failed to iterate child tuples. rc=%s", strrc(rc));
    return rc;
  }

  rc = RC::SUCCESS;
  for (size_t i = 0; i < old_records_.size() && rc == RC::SUCCESS; i++) {
    rc = trx_->update_record(table_, old_records_[i], new_records_[i]);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to update record by trx. rid=%s, rc=%s", old_records_[i].rid().to_string().c_str(), strrc(rc));
    }
  }

  return rc;
}

RC UpdatePhysicalOperator::next()
{
  return RC::RECORD_EOF;
}

RC UpdatePhysicalOperator::close()
{
  old_records_.clear();
  new_records_.clear();
  return RC::SUCCESS;
}

RC UpdatePhysicalOperator::apply_value_to_record(const Value &value, Record &record) const
{
  char *field_data = record.data() + field_meta_->offset();
  if (field_meta_->type() == AttrType::CHARS) {
    memset(field_data, 0, field_meta_->len());
  }

  size_t copy_len = field_meta_->len();
  if (field_meta_->type() == AttrType::CHARS && copy_len > static_cast<size_t>(value.length())) {
    copy_len = static_cast<size_t>(value.length()) + 1;
  }
  memcpy(field_data, value.data(), copy_len);
  return RC::SUCCESS;
}

