/*
 * Copyright (c) 2017, 2020, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms, as
 * designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef MODULES_UTIL_MOD_UTIL_H_
#define MODULES_UTIL_MOD_UTIL_H_

#include <memory>
#include <string>
#include <vector>

#include "mysqlshdk/libs/db/connection_options.h"

#include "mysqlshdk/include/scripting/types_cpp.h"

namespace shcore {
class IShell_core;
}

namespace mysqlsh {

/**
 * \defgroup util util
 * \ingroup ShellAPI
 * $(UTIL_BRIEF)
 */
class SHCORE_PUBLIC Util : public shcore::Cpp_object_bridge,
                           public std::enable_shared_from_this<Util> {
 public:
  explicit Util(shcore::IShell_core *owner);

  std::string class_name() const override { return "Util"; };

#if DOXYGEN_JS
  Undefined checkForServerUpgrade(ConnectionData connectionData,
                                  Dictionary options);
  Undefined checkForServerUpgrade(Dictionary options);
#elif DOXYGEN_PY
  None check_for_server_upgrade(ConnectionData connectionData, dict options);
  None check_for_server_upgrade(dict options);
#endif
  shcore::Value check_for_server_upgrade(const shcore::Argument_list &args);

#if DOXYGEN_JS
  Undefined importJson(String file, Dictionary options);
#elif DOXYGEN_PY
  None import_json(str file, dict options);
#endif
  void import_json(const std::string &file,
                   const shcore::Dictionary_t &options);

#if DOXYGEN_JS
  Undefined configureOci(String profile) {}
#elif DOXYGEN_PY
  None configure_oci(str profile);
#endif
  void configure_oci(const std::string &profile = "");

#if DOXYGEN_JS
  Undefined importTable(List filename, Dictionary options);
#elif DOXYGEN_PY
  None import_table(list filename, dict options);
#endif
  shcore::Value import_table(const shcore::Argument_list &args);

#if DOXYGEN_JS
  Undefined loadDump(String url, Dictionary options);
#elif DOXYGEN_PY
  None load_dump(str url, dict options);
#endif
  void load_dump(const std::string &url, const shcore::Dictionary_t &options);

#if DOXYGEN_JS
  Undefined exportTable(String table, String outputUrl, Dictionary options);
#elif DOXYGEN_PY
  None export_table(str table, str outputUrl, dict options);
#endif
  void export_table(const std::string &table, const std::string &file,
                    const shcore::Dictionary_t &options);

#if DOXYGEN_JS
  Undefined dumpTables(String schema, List tables, String outputUrl,
                       Dictionary options);
#elif DOXYGEN_PY
  None dump_tables(str schema, list tables, str outputUrl, dict options);
#endif
  void dump_tables(const std::string &schema,
                   const std::vector<std::string> &tables,
                   const std::string &directory,
                   const shcore::Dictionary_t &options);

#if DOXYGEN_JS
  Undefined dumpSchemas(List schemas, String outputUrl, Dictionary options);
#elif DOXYGEN_PY
  None dump_schemas(list schemas, str outputUrl, dict options);
#endif
  void dump_schemas(const std::vector<std::string> &schemas,
                    const std::string &directory,
                    const shcore::Dictionary_t &options);

#if DOXYGEN_JS
  Undefined dumpInstance(String outputUrl, Dictionary options);
#elif DOXYGEN_PY
  None dump_instance(str outputUrl, dict options);
#endif
  void dump_instance(const std::string &directory,
                     const shcore::Dictionary_t &options);

 private:
  shcore::IShell_core &_shell_core;
};

} /* namespace mysqlsh */

#endif  // MODULES_UTIL_MOD_UTIL_H_
