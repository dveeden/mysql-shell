#@ Cluster: validating members
|Cluster Members: 13|
|name: OK|
|get_name: OK|
|admin_type: OK|
|get_admin_type: OK|
|add_instance: OK|
|remove_instance: OK|
|rejoin_instance: OK|
|check_instance_state: OK|
|describe: OK|
|status: OK|
|help: OK|
|dissolve: OK|
|rescan: OK|

#@# Cluster: add_instance errors
||Invalid number of arguments in Cluster.add_instance, expected 1 to 2 but got 0
||Invalid number of arguments in Cluster.add_instance, expected 1 to 2 but got 4
||Cluster.add_instance: Invalid connection options, expected either a URI or a Dictionary
||Cluster.add_instance: Connection definition is empty
||Cluster.add_instance: Invalid and missing values in instance definition (invalid: weird), (missing: host)
||Cluster.add_instance: Invalid connection options, expected either a URI or a Dictionary
||Cluster.add_instance: Invalid values in instance definition: authMethod, schema
||Cluster.add_instance: Missing values in instance definition: host
||Cluster.add_instance: The instance '<<<localhost>>>:<<<__mysql_sandbox_port1>>>' already belongs to the ReplicaSet: 'default'

#@ Cluster: add_instance
||

#@<OUT> Cluster: describe1
{
    "adminType": "local",
    "clusterName": "devCluster",
    "defaultReplicaSet": {
        "instances": [
            {
                "host": "<<<localhost>>>:<<<__mysql_sandbox_port1>>>",
                "name": "<<<localhost>>>:<<<__mysql_sandbox_port1>>>",
                "role": "HA"
            },
            {
                "host": "<<<localhost>>>:<<<__mysql_sandbox_port2>>>",
                "name": "<<<localhost>>>:<<<__mysql_sandbox_port2>>>",
                "role": "HA"
            }
        ],
        "name": "default"
    }
}

#@<OUT> Cluster: status1
{
    "clusterName": "devCluster",
    "defaultReplicaSet": {
        "status": "Cluster is NOT tolerant to any failures.",
        "topology": {
            "<<<localhost>>>:<<<__mysql_sandbox_port1>>>": {
                "address": "<<<localhost>>>:<<<__mysql_sandbox_port1>>>",
                "leaves": {
                    "<<<localhost>>>:<<<__mysql_sandbox_port2>>>": {
                        "address": "<<<localhost>>>:<<<__mysql_sandbox_port2>>>",
                        "leaves": {

                        },
                        "mode": "R/O",
                        "role": "HA",
                        "status": "{{ONLINE|RECOVERING}}"
                    }
                },
                "mode": "R/W",
                "role": "HA",
                "status": "ONLINE"
            }
        }
    }
}


#@ Cluster: remove_instance errors
||Invalid number of arguments in Cluster.remove_instance, expected 1 but got 0
||Invalid number of arguments in Cluster.remove_instance, expected 1 but got 2
||Cluster.remove_instance: Invalid connection options, expected either a URI or a Dictionary
||Cluster.remove_instance: Invalid values in instance definition: authMethod, schema, user
||Cluster.remove_instance: The instance 'somehost:3306' does not belong to the ReplicaSet: 'default'

#@ Cluster: remove_instance
||

#@<OUT> Cluster: describe2
{
    "adminType": "local",
    "clusterName": "devCluster",
    "defaultReplicaSet": {
        "instances": [
            {
                "host": "<<<localhost>>>:<<<__mysql_sandbox_port1>>>",
                "name": "<<<localhost>>>:<<<__mysql_sandbox_port1>>>",
                "role": "HA"
            }
        ],
        "name": "default"
    }
}

#@<OUT> Cluster: status2
{
    "clusterName": "devCluster",
    "defaultReplicaSet": {
        "status": "Cluster is NOT tolerant to any failures.",
        "topology": {
            "<<<localhost>>>:<<<__mysql_sandbox_port1>>>": {
                "address": "<<<localhost>>>:<<<__mysql_sandbox_port1>>>",
                "leaves": {

                },
                "mode": "R/W",
                "role": "HA",
                "status": "ONLINE"
            }
        }
    }
}

#@ Cluster: addInstance 2 
||

#@<OUT> Cluster: describe after adding 2
{
    "adminType": "local", 
    "clusterName": "devCluster", 
    "defaultReplicaSet": {
        "instances": [
            {
                "host": "localhost:<<<__mysql_sandbox_port1>>>", 
                "name": "localhost:<<<__mysql_sandbox_port1>>>", 
                "role": "HA"
            },
            {
                "host": "localhost:<<<__mysql_sandbox_port2>>>", 
                "name": "localhost:<<<__mysql_sandbox_port2>>>", 
                "role": "HA"
            }
        ], 
        "name": "default"
    }
}


#@<OUT> Cluster: status after adding 2
{
    "clusterName": "devCluster", 
    "defaultReplicaSet": {
        "status": "Cluster is NOT tolerant to any failures.", 
        "topology": {
            "localhost:<<<__mysql_sandbox_port1>>>": {
                "address": "localhost:<<<__mysql_sandbox_port1>>>", 
                "leaves": {
                    "localhost:<<<__mysql_sandbox_port2>>>": {
                        "address": "localhost:<<<__mysql_sandbox_port2>>>", 
                        "leaves": {

                        }, 
                        "mode": "R/O", 
                        "role": "HA", 
                        "status": "{{ONLINE|RECOVERING}}"
                    }
                }, 
                "mode": "R/W", 
                "role": "HA", 
                "status": "ONLINE"
            }
        }
    }
}


#@ Cluster: remove_instance added
||

#@ Cluster: remove_instance last
||

#@<OUT> Cluster: describe3
{
    "adminType": "local",
    "clusterName": "devCluster",
    "defaultReplicaSet": {
        "instances": [
        ],
        "name": "default"
    }
}

#@<OUT> Cluster: status3
{
    "clusterName": "devCluster",
    "defaultReplicaSet": {
        "status": "Cluster is NOT tolerant to any failures.",
        "topology": {

        }
    }
}

#@ Cluster: dissolve errors
||Cluster.dissolve: Cannot drop cluster: The cluster is not empty
||Cluster.dissolve: Argument #1 is expected to be a map
||Invalid number of arguments in Cluster.dissolve, expected 0 to 1 but got 2
||Cluster.dissolve: Argument #1 is expected to be a map
||Cluster.dissolve: Invalid values in dissolve options: enforce
||Cluster.dissolve: Argument 'force' is expected to be a bool

#@ Cluster: dissolve
||

#@ Cluster: describe: dissolved cluster
||The cluster 'devCluster' no longer exists.

#@ Cluster: status: dissolved cluster
||The cluster 'devCluster' no longer exists.
