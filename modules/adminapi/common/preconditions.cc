/*
 * Copyright (c) 2018, 2020, Oracle and/or its affiliates.
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

#include "modules/adminapi/common/preconditions.h"
#include <mysqld_error.h>
#include <map>

#include "modules/adminapi/common/common.h"
#include "modules/adminapi/common/dba_errors.h"
#include "modules/adminapi/common/global_topology_check.h"
#include "modules/adminapi/common/instance_pool.h"
#include "modules/adminapi/common/sql.h"
#include "mysqlshdk/include/shellcore/scoped_contexts.h"
#include "mysqlshdk/libs/db/utils_error.h"
#include "mysqlshdk/libs/mysql/group_replication.h"

namespace mysqlsh {
namespace dba {

namespace {
// Holds a relation of messages for a given metadata state.
using MDS = metadata::State;

std::string lookup_message(const std::string &function_name, MDS state) {
  if (function_name == "*") {
    switch (state) {
      case MDS::MAJOR_HIGHER:
        return "The installed metadata version %s is higher than the supported "
               "by the Shell which is version %s. It is recommended to use a "
               "Shell version that supports this metadata.";
      case MDS::MAJOR_LOWER:
      case MDS::MINOR_LOWER:
      case MDS::PATCH_LOWER:
        return "The installed metadata version %s is lower than the version "
               "required by Shell which is version %s. It is recommended to "
               "upgrade the metadata. See \\? dba.<<<upgradeMetadata>>> for "
               "additional details.";
      case MDS::FAILED_UPGRADE:
        return metadata::kFailedUpgradeError;
      case MDS::UPGRADING:
        return "The metadata is being upgraded. Wait until the upgrade process "
               "completes and then retry the operation.";
      default:
        break;
    }
  } else if (function_name == "Dba.createCluster" ||
             function_name == "Dba.createReplicaSet") {
    switch (state) {
      case MDS::MAJOR_HIGHER:
        return "Operation not allowed. The installed metadata version %s is "
               "higher than the supported by the Shell which is version %s. "
               "Please use the latest version of the Shell.";
      case MDS::MAJOR_LOWER:
        return "Operation not allowed. The installed metadata version %s is "
               "lower than the version required by Shell which is version %s. "
               "Upgrade the metadata to execute this operation. See \\? "
               "dba.<<<upgradeMetadata>>> for additional details.";
      default:
        break;
    }
  } else if (function_name == "Dba.getCluster" ||
             function_name == "Dba.getReplicaSet") {
    Cluster_type type = function_name == "Dba.getCluster"
                            ? Cluster_type::GROUP_REPLICATION
                            : Cluster_type::ASYNC_REPLICATION;

    switch (state) {
      case MDS::MAJOR_HIGHER:
        return "No " + thing(type) +
               " change operations can be executed because the installed "
               "metadata version %s is higher than the supported by the Shell "
               "which is version %s. Please use the latest version of the "
               "Shell.";
      case MDS::MAJOR_LOWER:
        return "No " + thing(type) +
               " change operations can be executed because the installed "
               "metadata version %s is lower than the version required by "
               "Shell which is version %s. Upgrade the metadata to remove this "
               "restriction. See \\? dba.<<<upgradeMetadata>>> for additional "
               "details.";
      default:
        break;
    }
  } else if (function_name == "Dba.rebootClusterFromCompleteOutage") {
    switch (state) {
      case MDS::MAJOR_HIGHER:
        return "Operation not allowed. No " +
               thing(Cluster_type::GROUP_REPLICATION) +
               " change operations can be executed because the installed "
               "metadata version %s is higher than the supported by the Shell "
               "which is version %s. Please use the latest version of the "
               "Shell.";
      case MDS::MAJOR_LOWER:
        return "The " + thing(Cluster_type::GROUP_REPLICATION) +
               " will be rebooted as configured on the metadata, however, no "
               "change operations can be executed because the installed "
               "metadata version %s is lower than the version required by "
               "Shell which is version %s. Upgrade the metadata to remove this "
               "restriction. See \\? dba.<<<upgradeMetadata>>> for additional "
               "details.";
      default:
        break;
    }
  } else if (function_name == "Cluster" || function_name == "ReplicaSet") {
    Cluster_type type = function_name == "Cluster"
                            ? Cluster_type::GROUP_REPLICATION
                            : Cluster_type::ASYNC_REPLICATION;

    switch (state) {
      case MDS::MAJOR_HIGHER:
        return "Operation not allowed. No " + thing(type) +
               " change operations can be executed because the installed "
               "metadata version %s is higher than the supported by the Shell "
               "which is version %s. Please use the latest version of the "
               "Shell.";
      case MDS::MAJOR_LOWER:
        return "Operation not allowed. No " + thing(type) +
               " change operations can be executed because the installed "
               "metadata version %s is lower than the version required by "
               "Shell "
               "which is version %s. Upgrade the metadata to remove this "
               "restriction. See \\? dba.<<<upgradeMetadata>>> for additional "
               "details.";
      default:
        break;
    }
  }

  // If the message for the specific function is not found, then it tries
  // getting the message for the class. If the message for the class is not
  // found, then it tries getting the generic message.
  if (function_name != "*") {
    auto tokens = shcore::str_split(function_name, ".");
    if (tokens.size() == 2) {
      return lookup_message(tokens[0], state);
    } else {
      return lookup_message("*", state);
    }
  }
  return "";
}

// The replicaset functions do not use quorum
ReplicationQuorum::State na_quorum;
const std::map<std::string, FunctionAvailability>
    AdminAPI_function_availability = {
        // The Dba functions
        {"Dba.createCluster",
         {k_min_gr_version,
          InstanceType::Standalone | InstanceType::StandaloneWithMetadata |
              InstanceType::GroupReplication,
          ReplicationQuorum::State::any(),
          ManagedInstance::State::Any,
          {{metadata::kIncompatibleOrUpgrading, MDS_actions::RAISE_ERROR},
           {metadata::kCompatibleLower, MDS_actions::NOTE},
           {metadata::States(metadata::State::FAILED_SETUP),
            MDS_actions::NONE}}}},
        {"Dba.getCluster",
         {k_min_gr_version,
          InstanceType::InnoDBCluster,
          ReplicationQuorum::State::any(),
          ManagedInstance::State::Any,
          {{metadata::kUpgradeStates, MDS_actions::RAISE_ERROR},
           {metadata::kIncompatible, MDS_actions::WARN},
           {metadata::kCompatibleLower, MDS_actions::NOTE}}}},
        {"Dba.dropMetadataSchema",
         {k_min_adminapi_server_version,
          InstanceType::StandaloneWithMetadata |
              InstanceType::StandaloneInMetadata | InstanceType::InnoDBCluster |
              InstanceType::AsyncReplicaSet,
          ReplicationQuorum::State(ReplicationQuorum::States::Normal),
          ManagedInstance::State::OnlineRW | ManagedInstance::State::OnlineRO,
          {}}},
        {"Dba.rebootClusterFromCompleteOutage",
         {k_min_gr_version,
          InstanceType::StandaloneInMetadata | InstanceType::InnoDBCluster,
          ReplicationQuorum::State::any(),
          ManagedInstance::State::OnlineRW | ManagedInstance::State::OnlineRO,
          {{metadata::kUpgradeStates, MDS_actions::RAISE_ERROR},
           {metadata::States(metadata::State::MAJOR_HIGHER),
            MDS_actions::RAISE_ERROR},
           {metadata::States(metadata::State::MAJOR_LOWER), MDS_actions::WARN},
           {metadata::kCompatibleLower, MDS_actions::NOTE}}}},
        {"Dba.configureLocalInstance",
         {k_min_gr_version,
          InstanceType::Standalone | InstanceType::StandaloneWithMetadata |
              InstanceType::StandaloneInMetadata | InstanceType::InnoDBCluster |
              InstanceType::Unknown | InstanceType::GroupReplication,
          ReplicationQuorum::State::any(),
          ManagedInstance::State::Any,
          {}}},
        {"Dba.checkInstanceConfiguration",
         {k_min_gr_version,
          InstanceType::Standalone | InstanceType::StandaloneWithMetadata |
              InstanceType::StandaloneInMetadata | InstanceType::InnoDBCluster |
              InstanceType::GroupReplication | InstanceType::Unknown,
          ReplicationQuorum::State::any(),
          ManagedInstance::State::Any,
          {}}},
        {"Dba.configureInstance",
         {k_min_gr_version,
          InstanceType::Standalone | InstanceType::StandaloneWithMetadata |
              InstanceType::StandaloneInMetadata |
              InstanceType::GroupReplication | InstanceType::InnoDBCluster,
          ReplicationQuorum::State::any(),
          ManagedInstance::State::Any,
          {}}},

        {"Dba.upgradeMetadata",
         {k_min_gr_version,
          InstanceType::InnoDBCluster | InstanceType::AsyncReplicaSet,
          ReplicationQuorum::State(ReplicationQuorum::States::All_online),
          ManagedInstance::State::Any,
          {{metadata::kUpgradeInProgress, MDS_actions::RAISE_ERROR}}}},

        {"Dba.configureReplicaSetInstance",
         {k_min_ar_version,
          InstanceType::Standalone | InstanceType::StandaloneWithMetadata |
              InstanceType::StandaloneInMetadata |
              InstanceType::AsyncReplicaSet | InstanceType::Unknown,
          na_quorum,
          ManagedInstance::State::Any,
          {}}},
        {"Dba.createReplicaSet",
         {k_min_ar_version,
          InstanceType::Standalone | InstanceType::StandaloneWithMetadata,
          na_quorum,
          ManagedInstance::State::Any,
          {{metadata::kIncompatibleOrUpgrading, MDS_actions::RAISE_ERROR},
           {metadata::kCompatibleLower, MDS_actions::NOTE}}}},
        {"Dba.getReplicaSet",
         {k_min_ar_version,
          InstanceType::AsyncReplicaSet,
          na_quorum,
          ManagedInstance::State::Any,
          {{metadata::kUpgradeStates, MDS_actions::RAISE_ERROR},
           {metadata::kIncompatible, MDS_actions::WARN},
           {metadata::kCompatibleLower, MDS_actions::NOTE}}}},

        // GR Cluster functions
        {"Cluster.addInstance",
         {k_min_gr_version,
          InstanceType::InnoDBCluster,
          ReplicationQuorum::State(ReplicationQuorum::States::Normal),
          ManagedInstance::State::OnlineRW | ManagedInstance::OnlineRO,
          {{metadata::kIncompatibleOrUpgrading, MDS_actions::RAISE_ERROR}}}},
        {"Cluster.removeInstance",
         {k_min_gr_version,
          InstanceType::InnoDBCluster,
          ReplicationQuorum::State(ReplicationQuorum::States::Normal),
          ManagedInstance::State::OnlineRW | ManagedInstance::OnlineRO,
          {{metadata::kIncompatibleOrUpgrading, MDS_actions::RAISE_ERROR}}}},
        {"Cluster.rejoinInstance",
         {k_min_gr_version,
          InstanceType::InnoDBCluster,
          ReplicationQuorum::State(ReplicationQuorum::States::Normal),
          ManagedInstance::State::OnlineRW | ManagedInstance::State::OnlineRO,
          {{metadata::kIncompatibleOrUpgrading, MDS_actions::RAISE_ERROR}}}},
        {"Cluster.describe",
         {k_min_gr_version,
          InstanceType::InnoDBCluster,
          ReplicationQuorum::State::any(),
          ManagedInstance::State::Any,
          {{metadata::kUpgradeStates, MDS_actions::RAISE_ERROR}}}},
        {"Cluster.status",
         {k_min_gr_version,
          InstanceType::InnoDBCluster,
          ReplicationQuorum::State::any(),
          ManagedInstance::State::Any,
          {{metadata::kUpgradeStates, MDS_actions::RAISE_ERROR}}}},
        {"Cluster.resetRecoveryAccountsPassword",
         {k_min_gr_version,
          InstanceType::InnoDBCluster,
          ReplicationQuorum::State(ReplicationQuorum::States::Normal),
          ManagedInstance::State::OnlineRW | ManagedInstance::State::OnlineRO,
          {{metadata::kIncompatibleOrUpgrading, MDS_actions::RAISE_ERROR}}}},
        {"Cluster.options",
         {k_min_gr_version,
          InstanceType::InnoDBCluster,
          ReplicationQuorum::State::any(),
          ManagedInstance::State::Any,
          {{metadata::kUpgradeStates, MDS_actions::RAISE_ERROR}}}},
        {"Cluster.dissolve",
         {k_min_gr_version,
          InstanceType::InnoDBCluster,
          ReplicationQuorum::State(ReplicationQuorum::States::Normal),
          ManagedInstance::State::OnlineRW | ManagedInstance::OnlineRO,
          {{metadata::kIncompatibleOrUpgrading, MDS_actions::RAISE_ERROR}}}},
        {"Cluster.checkInstanceState",
         {k_min_gr_version,
          InstanceType::InnoDBCluster,
          ReplicationQuorum::State(ReplicationQuorum::States::Normal),
          ManagedInstance::State::OnlineRW | ManagedInstance::State::OnlineRO,
          {{metadata::kUpgradeStates, MDS_actions::RAISE_ERROR}}}},
        {"Cluster.rescan",
         {k_min_gr_version,
          InstanceType::InnoDBCluster,
          ReplicationQuorum::State(ReplicationQuorum::States::Normal),
          ManagedInstance::State::OnlineRW | ManagedInstance::OnlineRO,
          {{metadata::kIncompatibleOrUpgrading, MDS_actions::RAISE_ERROR}}}},
        {"Cluster.forceQuorumUsingPartitionOf",
         {k_min_gr_version,
          InstanceType::GroupReplication | InstanceType::InnoDBCluster,
          ReplicationQuorum::State::any(),
          ManagedInstance::State::OnlineRW | ManagedInstance::State::OnlineRO,
          {{metadata::kUpgradeStates, MDS_actions::RAISE_ERROR},
           {metadata::States(metadata::State::MAJOR_HIGHER),
            MDS_actions::RAISE_ERROR}}}},
        {"Cluster.switchToSinglePrimaryMode",
         {k_min_gr_version,
          InstanceType::InnoDBCluster,
          ReplicationQuorum::State(ReplicationQuorum::States::All_online),
          ManagedInstance::State::OnlineRW | ManagedInstance::State::OnlineRO,
          {{metadata::kIncompatibleOrUpgrading, MDS_actions::RAISE_ERROR}}}},
        {"Cluster.switchToMultiPrimaryMode",
         {k_min_gr_version,
          InstanceType::InnoDBCluster,
          ReplicationQuorum::State(ReplicationQuorum::States::All_online),
          ManagedInstance::State::OnlineRW | ManagedInstance::State::OnlineRO,
          {{metadata::kIncompatibleOrUpgrading, MDS_actions::RAISE_ERROR}}}},
        {"Cluster.setPrimaryInstance",
         {k_min_gr_version,
          InstanceType::InnoDBCluster,
          ReplicationQuorum::State(ReplicationQuorum::States::All_online),
          ManagedInstance::State::OnlineRW | ManagedInstance::State::OnlineRO,
          {{metadata::kIncompatibleOrUpgrading, MDS_actions::RAISE_ERROR}}}},
        {"Cluster.setOption",
         {k_min_gr_version,
          InstanceType::InnoDBCluster,
          ReplicationQuorum::State(ReplicationQuorum::States::All_online),
          ManagedInstance::State::OnlineRW | ManagedInstance::State::OnlineRO,
          {{metadata::kIncompatibleOrUpgrading, MDS_actions::RAISE_ERROR}}}},
        {"Cluster.setInstanceOption",
         {k_min_gr_version,
          InstanceType::InnoDBCluster,
          ReplicationQuorum::State(ReplicationQuorum::States::Normal),
          ManagedInstance::State::OnlineRW | ManagedInstance::State::OnlineRO,
          {{metadata::kIncompatibleOrUpgrading, MDS_actions::RAISE_ERROR}}}},
        {"Cluster.listRouters",
         {k_min_gr_version,
          InstanceType::InnoDBCluster,
          ReplicationQuorum::State::any(),
          ManagedInstance::State::Any,
          {{metadata::kUpgradeStates, MDS_actions::RAISE_ERROR}}}},
        {"Cluster.removeRouterMetadata",
         {k_min_gr_version,
          InstanceType::InnoDBCluster,
          ReplicationQuorum::State::any(),
          ManagedInstance::State::Any,
          {{metadata::kUpgradeStates, MDS_actions::RAISE_ERROR}}}},
        {"Cluster.setupAdminAccount",
         {k_min_gr_version,
          InstanceType::InnoDBCluster,
          ReplicationQuorum::State(ReplicationQuorum::States::Normal),
          ManagedInstance::State::OnlineRW | ManagedInstance::State::OnlineRO,
          {{metadata::kUpgradeStates, MDS_actions::RAISE_ERROR}}}},
        {"Cluster.setupRouterAccount",
         {k_min_gr_version,
          InstanceType::InnoDBCluster,
          ReplicationQuorum::State(ReplicationQuorum::States::Normal),
          ManagedInstance::State::OnlineRW | ManagedInstance::State::OnlineRO,
          {{metadata::kUpgradeStates, MDS_actions::RAISE_ERROR}}}},

        // ReplicaSet Functions
        {"ReplicaSet.addInstance",
         {k_min_ar_version,
          InstanceType::AsyncReplicaSet,
          na_quorum,
          ManagedInstance::State::OnlineRW | ManagedInstance::State::OnlineRO,
          {{metadata::kIncompatibleOrUpgrading, MDS_actions::RAISE_ERROR}}}},
        {"ReplicaSet.rejoinInstance",
         {k_min_ar_version,
          InstanceType::AsyncReplicaSet,
          na_quorum,
          ManagedInstance::State::OnlineRW | ManagedInstance::State::OnlineRO,
          {{metadata::kIncompatibleOrUpgrading, MDS_actions::RAISE_ERROR}}}},
        {"ReplicaSet.removeInstance",
         {k_min_ar_version,
          InstanceType::AsyncReplicaSet,
          na_quorum,
          ManagedInstance::State::OnlineRW | ManagedInstance::State::OnlineRO,
          {{metadata::kIncompatibleOrUpgrading, MDS_actions::RAISE_ERROR}}}},
        {"ReplicaSet.describe",
         {k_min_ar_version,
          InstanceType::AsyncReplicaSet,
          na_quorum,
          ManagedInstance::State::Any,
          {{metadata::kUpgradeStates, MDS_actions::RAISE_ERROR}}}},
        {"ReplicaSet.status",
         {k_min_ar_version,
          InstanceType::AsyncReplicaSet,
          na_quorum,
          ManagedInstance::State::Any,
          {{metadata::kUpgradeStates, MDS_actions::RAISE_ERROR}}}},
        {"ReplicaSet.dissolve",
         {k_min_ar_version,
          InstanceType::AsyncReplicaSet,
          na_quorum,
          ManagedInstance::State::OnlineRW,
          {{metadata::kIncompatibleOrUpgrading, MDS_actions::RAISE_ERROR}}}},
        {"ReplicaSet.checkInstanceState",
         {k_min_ar_version,
          InstanceType::AsyncReplicaSet,
          na_quorum,
          ManagedInstance::State::OnlineRW | ManagedInstance::State::OnlineRO,
          {}}},
        {"ReplicaSet.setPrimaryInstance",
         {k_min_ar_version,
          InstanceType::AsyncReplicaSet,
          na_quorum,
          ManagedInstance::State::Any,
          {{metadata::kIncompatibleOrUpgrading, MDS_actions::RAISE_ERROR}}}},
        {"ReplicaSet.forcePrimaryInstance",
         {k_min_ar_version,
          InstanceType::AsyncReplicaSet,
          na_quorum,
          ManagedInstance::State::Any,
          {{metadata::kIncompatibleOrUpgrading, MDS_actions::RAISE_ERROR}}}},
        {"ReplicaSet.listRouters",
         {k_min_ar_version,
          InstanceType::AsyncReplicaSet,
          na_quorum,
          ManagedInstance::State::Any,
          {{metadata::kUpgradeStates, MDS_actions::RAISE_ERROR}}}},
        {"ReplicaSet.removeRouterMetadata",
         {k_min_ar_version,
          InstanceType::AsyncReplicaSet,
          na_quorum,
          ManagedInstance::State::Any,
          {{metadata::kUpgradeStates, MDS_actions::RAISE_ERROR}}}},
        {"ReplicaSet.setupAdminAccount",
         {k_min_ar_version,
          InstanceType::AsyncReplicaSet,
          na_quorum,
          ManagedInstance::State::OnlineRW | ManagedInstance::State::OnlineRO,
          {{metadata::kUpgradeStates, MDS_actions::RAISE_ERROR}}}},
        {"ReplicaSet.setupRouterAccount",
         {k_min_ar_version,
          InstanceType::AsyncReplicaSet,
          na_quorum,
          ManagedInstance::State::OnlineRW | ManagedInstance::State::OnlineRO,
          {{metadata::kUpgradeStates, MDS_actions::RAISE_ERROR}}}},
        {"ReplicaSet.setOption",
         {k_min_ar_version,
          InstanceType::AsyncReplicaSet,
          na_quorum,
          ManagedInstance::State::OnlineRW | ManagedInstance::State::OnlineRO,
          {{metadata::kIncompatibleOrUpgrading, MDS_actions::RAISE_ERROR}}}},
        {"ReplicaSet.setInstanceOption",
         {k_min_ar_version,
          InstanceType::AsyncReplicaSet,
          na_quorum,
          ManagedInstance::State::OnlineRW | ManagedInstance::State::OnlineRO,
          {{metadata::kIncompatibleOrUpgrading, MDS_actions::RAISE_ERROR}}}},
        {"ReplicaSet.options",
         {k_min_ar_version,
          InstanceType::AsyncReplicaSet,
          na_quorum,
          ManagedInstance::State::Any,
          {{metadata::kUpgradeStates, MDS_actions::RAISE_ERROR}}}},
};

}  // namespace

/**
 * Validates the session for AdminAPI operations.
 *
 * Checks if the given session exists and is open and if the session version
 * is valid for use with AdminAPI.
 *
 * @param group_session A session to validate.
 *
 * @throws shcore::Exception::runtime_error if session is not valid.
 */
void validate_session(const std::shared_ptr<mysqlshdk::db::ISession> &session) {
  // A classic session is required to perform any of the AdminAPI operations
  if (!session) {
    throw shcore::Exception::runtime_error(
        "An open session is required to perform this operation");
  } else if (!session->is_open()) {
    throw shcore::Exception::runtime_error(
        "The session was closed. An open session is required to perform "
        "this operation");
  }

  // Validate if the server version is supported by the AdminAPI
  mysqlshdk::utils::Version server_version = session->get_server_version();

  if (server_version >= k_max_adminapi_server_version ||
      server_version < k_min_adminapi_server_version)
    throw shcore::Exception::runtime_error(
        "Unsupported server version: AdminAPI operations require MySQL "
        "server versions 5.7 or 8.0");
}

/**
 * Validates the session used to manage the group.
 *
 * Checks if the given session is valid for use with AdminAPI.
 *
 * @param instance A group session to validate.
 *
 * @throws shcore::Exception::runtime_error if session is not valid.
 */
void validate_gr_session(
    const std::shared_ptr<mysqlshdk::db::ISession> &group_session) {
  // TODO(alfredo) - this check seems too extreme, this only matters in the
  // target instance (and only for some operations) and this function can get
  // called on other members
  if (mysqlshdk::gr::is_group_replication_delayed_starting(
          mysqlsh::dba::Instance(group_session)))
    throw shcore::Exception::runtime_error(
        "Cannot perform operation while group replication is "
        "starting up");
}

bool check_metadata(const MetadataStorage &metadata,
                    mysqlshdk::utils::Version *out_version,
                    Cluster_type *out_type) {
  if (metadata.check_version(out_version)) {
    auto target_server = metadata.get_md_server();
    log_debug("Instance type check: %s: Metadata version %s found",
              target_server->descr().c_str(), out_version->get_full().c_str());

    if (!metadata.check_instance_type(target_server->get_uuid(), *out_version,
                                      out_type)) {
      *out_type = Cluster_type::NONE;
      log_debug("Instance %s is not managed",
                target_server->get_uuid().c_str());
    } else {
      log_debug("Instance %s is managed for %s",
                target_server->get_uuid().c_str(),
                to_string(*out_type).c_str());
    }

    return true;
  }
  return false;
}

bool check_group_replication_active(
    const std::shared_ptr<Instance> &target_server) {
  bool ret_val = false;

  auto result = target_server->query(
      "select count(*) "
      "from performance_schema.replication_group_members "
      "where MEMBER_ID = @@server_uuid AND MEMBER_STATE IS "
      "NOT NULL AND MEMBER_STATE <> 'OFFLINE'");
  auto row = result->fetch_one();
  if (row) {
    if (row->get_int(0) != 0) {
      log_debug("Instance type check: %s: GR is active",
                target_server->descr().c_str());
      ret_val = true;
    } else {
      log_debug("Instance type check: %s: GR is installed but not active",
                target_server->descr().c_str());
    }
  }

  return ret_val;
}

InstanceType::Type get_instance_type(const MetadataStorage &metadata) {
  mysqlshdk::utils::Version md_version;
  Cluster_type cluster_type;
  bool has_metadata = false;
  bool gr_active;
  auto target_server = metadata.get_md_server();

  try {
    has_metadata = check_metadata(metadata, &md_version, &cluster_type);

    gr_active = check_group_replication_active(target_server);
  } catch (const shcore::Error &error) {
    auto e =
        shcore::Exception::mysql_error_with_code(error.what(), error.code());

    log_warning("Error querying GR member state: %s: %i %s",
                target_server->descr().c_str(), error.code(), error.what());

    if (error.code() == ER_NO_SUCH_TABLE)
      gr_active = false;
    else if (error.code() == ER_TABLEACCESS_DENIED_ERROR) {
      throw std::runtime_error(
          "Unable to detect target instance state. Please check account "
          "privileges.");
    } else {
      throw shcore::Exception::mysql_error_with_code(error.what(),
                                                     error.code());
    }
  }

  if (has_metadata) {
    if (cluster_type == Cluster_type::GROUP_REPLICATION && gr_active) {
      return InstanceType::InnoDBCluster;
    } else if (cluster_type == Cluster_type::GROUP_REPLICATION && !gr_active) {
      // InnoDB cluster but with GR stopped
      return InstanceType::StandaloneInMetadata;
    } else if (gr_active) {
      // GR running but instance is not in the metadata, could be:
      // - member was added to the cluster by hand
      // - UUID of member changed
      // - MD in the group belongs to a different cluster
      // - others
      if (cluster_type != Cluster_type::NONE)
        log_warning(
            "Instance %s is running Group Replication, but does not belong "
            "to a InnoDB cluster",
            target_server->descr().c_str());
      return InstanceType::GroupReplication;
    } else {
      if (cluster_type == Cluster_type::ASYNC_REPLICATION)
        return InstanceType::AsyncReplicaSet;

      return InstanceType::StandaloneWithMetadata;
    }
  } else {
    if (gr_active) return InstanceType::GroupReplication;
  }

  return InstanceType::Standalone;
}

namespace ManagedInstance {
std::string describe(State state) {
  std::string ret_val;
  switch (state) {
    case OnlineRW:
      ret_val = "Read/Write";
      break;
    case OnlineRO:
      ret_val = "Read Only";
      break;
    case Recovering:
      ret_val = "Recovering";
      break;
    case Unreachable:
      ret_val = "Unreachable";
      break;
    case Offline:
      ret_val = "Offline";
      break;
    case Error:
      ret_val = "Error";
      break;
    case Missing:
      ret_val = "(Missing)";
      break;
    case Any:
      assert(0);  // FIXME(anyone) avoid using enum as a bitmask
      break;
  }
  return ret_val;
}
}  // namespace ManagedInstance

Cluster_check_info get_cluster_check_info(const MetadataStorage &metadata) {
  validate_session(metadata.get_md_server()->get_session());

  Cluster_check_info state;
  auto group_server = metadata.get_md_server();

  // Retrieves the instance configuration type from the perspective of the
  // active session
  try {
    state.source_type = get_instance_type(metadata);
  } catch (const shcore::Exception &e) {
    if (mysqlshdk::db::is_server_connection_error(e.code())) {
      throw;
    } else {
      log_warning("Error detecting GR instance: %s", e.what());
      state.source_type = InstanceType::Unknown;
    }
  }

  // If it is a GR instance, validates the instance state
  if (state.source_type == InstanceType::GroupReplication ||
      state.source_type == InstanceType::InnoDBCluster) {
    validate_gr_session(group_server->get_session());

    // Retrieves the instance cluster statues from the perspective of the
    // active session
    state = get_replication_group_state(*group_server, state.source_type);

    // On IDC we want to also determine whether the quorum is just Normal or
    // if All the instances are ONLINE
    if (state.source_type == InstanceType::InnoDBCluster &&
        state.quorum == ReplicationQuorum::States::Normal) {
      try {
        if (metadata.check_all_members_online()) {
          state.quorum |= ReplicationQuorum::States::All_online;
        }
      } catch (const shcore::Error &e) {
        log_error(
            "Error while verifying all members in InnoDB Cluster are ONLINE: "
            "%s",
            e.what());
        throw;
      }
    }
  } else if (state.source_type == InstanceType::AsyncReplicaSet) {
    auto instance = metadata.get_instance_by_uuid(group_server->get_uuid());
    if (instance.primary_master)
      state.source_state = ManagedInstance::OnlineRW;
    else
      state.source_state = ManagedInstance::OnlineRO;
    state.quorum = ReplicationQuorum::States::Normal;
  } else {
    state.quorum = ReplicationQuorum::States::Normal;
    state.source_state = ManagedInstance::Offline;
  }

  state.source_version = group_server->get_version();

  return state;
}

void check_preconditions(const std::string &function_name,
                         const Cluster_check_info &state,
                         FunctionAvailability *custom_func_avail) {
  FunctionAvailability availability;
  // If a FunctionAvailability is not provided, look it up based on the function
  // name
  if (!custom_func_avail) {
    // Retrieves the availability configuration for the given function
    assert(AdminAPI_function_availability.find(function_name) !=
           AdminAPI_function_availability.end());
    availability = AdminAPI_function_availability.at(function_name);
  } else {
    // If a function availability is provided use it
    availability = *custom_func_avail;
  }
  std::string error;
  int code = 0;

  // Check minimum version for the specific function
  if (availability.min_version > state.source_version) {
    throw shcore::Exception::runtime_error(
        "Unsupported server version: This AdminAPI operation requires MySQL "
        "version " +
        availability.min_version.get_full() + " or newer, but target is " +
        state.source_version.get_full());
  }

  // Retrieves the instance configuration type from the perspective of the
  // active session
  // Validates availability based on the configuration state
  if (state.source_type & availability.instance_config_state) {
    // If it is a GR instance, validates the instance state
    if (state.source_type == InstanceType::GroupReplication ||
        state.source_type == InstanceType::InnoDBCluster ||
        state.source_type == InstanceType::AsyncReplicaSet) {
      // Validates availability based on the instance status
      if (state.source_state & availability.instance_status) {
        // Finally validates availability based on the Cluster quorum for IDC
        // operations
        if (state.source_type != InstanceType::AsyncReplicaSet &&
            !state.quorum.matches_any(availability.cluster_status)) {
          if (state.quorum.is_set(ReplicationQuorum::States::Normal)) {
            if (availability.cluster_status.is_set(
                    ReplicationQuorum::States::All_online)) {
              error =
                  "This operation requires all the cluster members to be "
                  "ONLINE";
            } else {
              error = "Unable to perform this operation";
            }
          } else if (state.quorum.is_set(
                         ReplicationQuorum::States::Quorumless)) {
            error = "There is no quorum to perform the operation";
            code = SHERR_DBA_GROUP_HAS_NO_QUORUM;
          } else if (state.quorum.is_set(ReplicationQuorum::States::Dead)) {
            error =
                "Unable to perform the operation on a dead InnoDB "
                "cluster";
          }
        }
      } else {
        error = "This function is not available through a session";

        switch (state.source_state) {
          case ManagedInstance::OnlineRO:
            error += " to a read only instance";
            break;
          case ManagedInstance::Offline:
            error += " to an offline instance";
            break;
          case ManagedInstance::Error:
            error += " to an instance in error state";
            break;
          case ManagedInstance::Recovering:
            error += " to a recovering instance";
            break;
          case ManagedInstance::Unreachable:
            error += " to an unreachable instance";
            break;
          default:
            // no-op
            break;
        }
      }
    }
  } else {
    error = "This function is not available through a session";
    switch (state.source_type) {
      case InstanceType::Unknown:
        // The original failure detecting the instance type gets logged.
        error =
            "Unable to detect target instance state. Please see the shell log "
            "for more details.";
        break;
      case InstanceType::Standalone:
        error += " to a standalone instance";
        code = SHERR_DBA_BADARG_INSTANCE_NOT_MANAGED;
        break;
      case InstanceType::StandaloneWithMetadata:
        if (availability.instance_config_state &
            InstanceType::AsyncReplicaSet) {
          error +=
              " to a standalone instance (metadata exists, instance does not "
              "belong to that metadata)";
        } else {
          error +=
              " to a standalone instance (metadata exists, instance does not "
              "belong to that metadata, and GR is not active)";
        }
        break;
      case InstanceType::StandaloneInMetadata:
        if (availability.instance_config_state &
            InstanceType::AsyncReplicaSet) {
          error +=
              " to a standalone instance (metadata exists, instance belongs to "
              "that metadata)";
        } else {
          error +=
              " to a standalone instance (metadata exists, instance belongs to "
              "that metadata, but GR is not active)";
        }
        code = SHERR_DBA_BADARG_INSTANCE_NOT_ONLINE;
        break;
      case InstanceType::GroupReplication:
        error +=
            " to an instance belonging to an unmanaged replication "
            "group";
        break;
      case InstanceType::InnoDBCluster:
        error += " to an instance already in an InnoDB cluster";
        code = SHERR_DBA_BADARG_INSTANCE_MANAGED_IN_CLUSTER;
        break;

      case InstanceType::AsyncReplicaSet:
        error += " to an instance that is a member of an InnoDB ReplicaSet";
        code = SHERR_DBA_BADARG_INSTANCE_MANAGED_IN_REPLICASET;
        break;
    }
  }

  if (!error.empty()) {
    if (code != 0)
      throw shcore::Exception(error, code);
    else
      throw shcore::Exception::runtime_error(error);
  }
}

/**
 * This function verifies the installed metadata vs the supported metadata.
 * The action to be taken depends on the metadata options configured for each
 * function:
 *
 * If the metadata state is Compatible no action will be taken.
 *
 * If the metadata is not Compatible, but any of the other states the action
 * depends on the configuration for each function as follows:
 *
 * If no specific actions are configured for the function:
 * - Read_compatible will be treated as Incompatible
 * - If metadata is Incompatible an error will be shown indicating whether:
 *   - The metadata Should be updated
 *   - The Shell should be updated
 *
 * If specific actions are configured they will be executed as configured.
 *
 * Every function requiring specific action handling should have it registered
 * on the AdminAPI_function_availability.
 */
MDS check_metadata_preconditions(const std::string &function_name,
                                 const MetadataStorage &metadata) {
  // It is assumed that if metadata is Compatible, there should be no
  // restrictions on any function
  assert(AdminAPI_function_availability.find(function_name) !=
         AdminAPI_function_availability.end());
  FunctionAvailability availability =
      AdminAPI_function_availability.at(function_name);

  // Metadata validation is done only on the functions that require it
  if (!availability.metadata_validations.empty()) {
    MDS compatibility = metadata.state();

    if (compatibility != MDS::EQUAL) {
      for (const auto &validation : availability.metadata_validations) {
        if (validation.state.is_set(compatibility)) {
          // Gets the right message for the function on this state
          std::string msg = lookup_message(function_name, compatibility);

          // If the message is empty then no action is required, falls back to
          // existing precondition checks.
          if (!msg.empty()) {
            auto pre_formatted = shcore::str_format(
                msg.c_str(), metadata.installed_version().get_base().c_str(),
                mysqlsh::dba::metadata::current_version().get_base().c_str());

            auto console = mysqlsh::current_console();
            if (validation.action == MDS_actions::WARN) {
              console->print_warning(pre_formatted);
            } else if (validation.action == MDS_actions::NOTE) {
              console->print_note(pre_formatted);
            } else if (validation.action == MDS_actions::RAISE_ERROR) {
              throw std::runtime_error(shcore::str_subvars(
                  pre_formatted,
                  [](const std::string &var) {
                    return shcore::get_member_name(
                        var, shcore::current_naming_style());
                  },
                  "<<<", ">>>"));
            }
          }
        }
      }
    }
    return compatibility;
  }
  return MDS::EQUAL;
}

Cluster_check_info check_function_preconditions(
    const std::string &function_name,
    const std::shared_ptr<Instance> &group_server,
    FunctionAvailability *custom_func_avail) {
  assert(function_name.find('.') != std::string::npos);

  if (!group_server || !group_server->get_session() ||
      !group_server->get_session()->is_open())
    throw shcore::Exception::runtime_error(
        "An open session is required to perform this operation.");

  MetadataStorage metadata(group_server);

  // Performs metadata state validations before anything else
  MDS mds = check_metadata_preconditions(function_name, metadata);

  Cluster_check_info info = get_cluster_check_info(metadata);

  // bypass the checks if MD setup failed and it's allowed (we're recovering)
  if (mds != MDS::FAILED_SETUP)
    check_preconditions(function_name, info, custom_func_avail);

  return info;
}

Cluster_check_info check_function_preconditions(
    const std::string &function_name,
    const std::shared_ptr<MetadataStorage> &metadata,
    FunctionAvailability *custom_func_avail) {
  assert(function_name.find('.') != std::string::npos);

  // Performs metadata state validations before anything else
  MDS mds = check_metadata_preconditions(function_name, *metadata.get());

  Cluster_check_info info = get_cluster_check_info(*metadata.get());

  // bypass the checks if MD setup failed and it's allowed (we're recovering)
  if (mds != MDS::FAILED_SETUP)
    check_preconditions(function_name, info, custom_func_avail);

  return info;
}

}  // namespace dba
}  // namespace mysqlsh
