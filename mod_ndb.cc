/* Copyright (C) 2006 MySQL AB

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/


#include "mod_ndb.h"

//
// Forward declarations for this file only: */
//
     /* Single-threaded (Apache 1.3) version: */
ndb_instance *instance1;
struct mod_ndb_process process;
int ndb_force_send = 1;


//
// INITIALIZATION & CLEAN-UP FUNCTIONS:
// child_init, child_exit, and init_instance
//
void mod_ndb_child_init(server_rec *s, pool *p) {

  /* Initialize the NDB API */
  ndb_init();

  /* Get server configuration */
  config::srv *srv = (config::srv *)
    ap_get_module_config(s->module_config, &ndb_module);

  /* Connect to the cluster */
  connect_to_cluster(& process.conn, s, srv);

  /* Initialize the process structure */
  process.n_connections = 1;
  process.n_threads = 1;
  process.conn.instances = & instance1;

  /* Create an ndb instance */
  instance1 = (ndb_instance *) ap_pcalloc(p, sizeof(ndb_instance));
  init_instance(& process.conn , instance1);
}


//
// child_exit
//
void mod_ndb_child_exit(server_rec *s, pool *p) {
  ndb_connection *c;
  ndb_instance *i;
  int n = 0, id = 0;
  
  for(c = & process.conn; c != (ndb_connection *) 0 ; c=c->next) {
    if(c->connection != 0) {
      id = c->connection->node_id();
      
      /* These were allocated by the C++ runtime, so let C++ free them,
         e.g. during "apachectl graceful"
      */    
      for(i = c->instances[n] ; n < process.n_threads ; n++)
          delete i->db;
      delete c->connection;

      ap_log_error(APLOG_MARK, log::err, s, 
                   "Node %d disconnected from cluster.", id);
    }
  }

  /* Shut down the NDB API */
  ndb_end(0);  
}


/* connect_to_cluster is always called from child_init(),
   but it might also be called from my_instance(), if different vhosts
   connect to different clusters, or if initialization failed the first time.
*/
void connect_to_cluster(ndb_connection *c, server_rec *s, config::srv *srv) {
  
  /* The C++ runtime allocates an Ndb_cluster_connection here */
  c->connection = new Ndb_cluster_connection((srv->connect_string));

  // To do: arguments to connnect() ???
  if (c->connection->connect()) {
    /* If the cluster is down, there could be a flood of these,
    so write it as a warning to maybe prevent filling up a log file. */
    log_err(s,"Cannot connect to NDB Cluster.");
    return;
  }

  if((c->connection->wait_until_ready(20, 0)) < 0) {
    log_err(s,"Timeout waiting for cluster to become ready.");
    return;
  }
  
  /* Succesfully connected */
  c->connected=1;
  ap_log_error(APLOG_MARK, log::err, s, 
               "Process %d connected to NDB Cluster as node %d", 
               getpid(), c->connection->node_id());

  /* In multi-threaded apache 2 this might be configurable */
  c->ndb_force_send = ndb_force_send;

  return;
}


void init_instance(ndb_connection *c, ndb_instance *i) {
  
  /* The C++ runtime allocates an Ndb object here */
  i->db = new Ndb(c->connection);
  
  /* init(n) where n is max no. of active transactions; default is 4 */
  i->db->init();
  
  /* i->conn is a pointer back to the parent connection */
  i->conn = c;
  
  /* Instances are allocated with ap_pcalloc(), so all counters 
     have been initialized to zero.
  */
  
  return;
}


ndb_instance *my_instance(request_rec *r) {
  ndb_connection *c = & process.conn;
  
  config::srv *srv = (config::srv *)
  ap_get_module_config(r->server->module_config, &ndb_module);
  
  
  /* This is the common case: */
  if(process.n_connections == 1) {
    if(c->connected == 0) {
      connect_to_cluster(c, r->server, srv);
      if(! c->connected) return (ndb_instance *) 0;
    }
    return c->instances[0];
  }
  
  /* To do: implement the case with multiple connect strings */
  ap_log_error(APLOG_MARK, log::warn, r->server,
               "Unwritten code in mod_ndb.cc at line %d!", __LINE__);
  
  return (ndb_instance *) 0;
}


//
// Configuration
//

namespace config {

  command_rec commands[] =
  {
      {   /* Per-server: ndb-connectstring */
        "ndb-connectstring",          
        (CMD_HAND_TYPE) ap_set_string_slot,
        (void *)XtOffsetOf(config::srv, connect_string),
        RSRC_CONF,      TAKE1, 
        "NDB Connection String" 
      },
      {
        "Database",         // inheritable
        (CMD_HAND_TYPE) ap_set_string_slot,
        (void *)XtOffsetOf(config::dir, database),
        ACCESS_CONF,    TAKE1, 
        "MySQL database schema" 
      },
      {
        "Table",            // inheritable
        (CMD_HAND_TYPE) ap_set_string_slot,
        (void *)XtOffsetOf(config::dir, table),
        ACCESS_CONF,    TAKE1, 
        "NDB Table"
      },            
      {
        "Deletes",          // NOT inheritable, defaults to 0
        (CMD_HAND_TYPE) ap_set_flag_slot,
        (void *)XtOffsetOf(config::dir, allow_delete),
        ACCESS_CONF,     FLAG,
        "Allow DELETE over HTTP"
      },
      {
        "Format",           // inheritable
        (CMD_HAND_TYPE) config::result_format,
        NULL,
        ACCESS_CONF,    TAKE123, 
        "Result Set Format"
      },     
      {
        "Columns",          // inheritable
        (CMD_HAND_TYPE) config::non_key_column,
        (void *) "R",
        ACCESS_CONF,    ITERATE,
        "List of attributes to include in result set"
      },
      {
        "AllowUpdate",      // NOT inheritable
        (CMD_HAND_TYPE) config::non_key_column,
        (void *) "W",
        ACCESS_CONF,    ITERATE,
        "List of attributes that can be updated using HTTP"
      },
      {
        "PrimaryKey",      // NOT inheritable
        (CMD_HAND_TYPE) config::primary_key,
        (void *) "P",  
        ACCESS_CONF,    ITERATE,
        "Allow Primary Key lookups"
      },
      {
        "UniqueIndex",      // NOT inheritable
        (CMD_HAND_TYPE) config::named_index,
        (void *) "U",  
        ACCESS_CONF,    ITERATE2,
        "Allow unique key lookups, given index name and columns"
      },
      {
        "OrderedIndex",      // NOT inheritable
        (CMD_HAND_TYPE) config::named_index,
        (void *) "O",  
        ACCESS_CONF,    ITERATE2,
        "Allow ordered index scan, given index name and columns"
      },
      {                      // inheritable
        "PathInfo", // e.g. "PathInfo id" or "PathInfo keypt1/keypt2"
        (CMD_HAND_TYPE) config::pathinfo,  
        NULL,
        ACCESS_CONF,    TAKE1, 
        "Schema for interpreting the rightmost URL path components"
      },
      {                     // NOT inheritable
        "Filter",  // Filter col op keyword, e.g. "Filter id < min_id"
        (CMD_HAND_TYPE) config::filter, 
        NULL,
        ACCESS_CONF,    TAKE13, 
        "NDB Table"
      }, 
    
     {NULL, NULL, NULL, 0, cmd_how(0), NULL}
  };
}


extern "C" {

  /* HANDLER LIST */
  
  /* The handlers are in handlers.cc: */

  extern int ndb_handler(request_rec *);
  extern int ndb_status_handler(request_rec *);
  extern int ndb_config_check_handler(request_rec *);

  static const handler_rec mod_ndb_handlers[] = { 
      { "ndb-cluster", ndb_handler },
      { "ndb-status", ndb_status_handler },
      { "ndb-config-check", ndb_config_check_handler },
      { NULL, NULL }
  };

  /************************
   * Global Dispatch List *
   ************************/

  module MODULE_VAR_EXPORT ndb_module = {
    STANDARD_MODULE_STUFF, 
    NULL,                       /* module initializer                  */
    config::init_dir,           /* create per-dir    config structures */
    config::merge_dir,          /* merge  per-dir    config structures */
    config::init_srv,           /* create per-server config structures */
    NULL,                       /* merge  per-server config structures */
    config::commands,           /* table of config file commands       */
    mod_ndb_handlers,           /* [#8] MIME-typed-dispatched handlers */
    NULL,                       /* [#1] URI to filename translation    */
    NULL,                       /* [#4] validate user id from request  */
    NULL,                       /* [#5] check if the user is ok _here_ */
    NULL,                       /* [#3] check access by host address   */
    NULL,                       /* [#6] determine MIME type            */
    NULL,                       /* [#7] pre-run fixups                 */
    NULL,                       /* [#9] log a transaction              */
    NULL,                       /* [#2] header parser                  */
    mod_ndb_child_init,         /* child_init                          */
    mod_ndb_child_exit,         /* child_exit                          */
    NULL                        /* [#0] post read-request              */
  #ifdef EAPI
    ,NULL,                      /* EAPI: add_module                    */
    NULL,                       /* EAPI: remove_module                 */
    NULL,                       /* EAPI: rewrite_command               */
    NULL,                       /* EAPI: new_connection                */
    NULL                        /* EAPI: close_connection              */
  #endif
  };
};