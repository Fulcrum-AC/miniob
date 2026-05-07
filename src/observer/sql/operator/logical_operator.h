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
// Created by Wangyunlai on 2022/12/07.
//

#pragma once

#include "sql/expr/expression.h"
#include "sql/operator/operator_node.h"
#include "common/lang/unordered_set.h"

/**
 * @brief Logical operator type.
 */
enum class LogicalOperatorType
{
  CALC,
  TABLE_GET,
  PREDICATE,
  PROJECTION,
  JOIN,
  INSERT,
  DELETE,
  UPDATE,
  EXPLAIN,
  GROUP_BY,
};

/**
 * @brief Base class of logical operators.
 */
class LogicalOperator : public OperatorNode
{
public:
  LogicalOperator() = default;
  virtual ~LogicalOperator();

  virtual LogicalOperatorType type() const = 0;

  bool is_physical() const override { return false; }
  bool is_logical() const override { return true; }

  void add_child(unique_ptr<LogicalOperator> oper);
  void add_expressions(unique_ptr<Expression> expr);
  auto children() -> vector<unique_ptr<LogicalOperator>> & { return children_; }
  auto expressions() -> vector<unique_ptr<Expression>> & { return expressions_; }

  static bool can_generate_vectorized_operator(const LogicalOperatorType &type);

  // TODO: used by cascade optimizer, temporary function.
  void generate_general_child();

protected:
  vector<unique_ptr<LogicalOperator>> children_;
  vector<unique_ptr<Expression>>      expressions_;
};

