/*
 * Experimental Interface to MongoDB based on nsf (Next Scripting
 * Framework)
 * 
 * The example shows how to use the source code generator from nsf to
 * generate a c interface. 

 * This implementation provides a low level interface based on tagged
 * elements to force / preserve the datatypes of mongodb. 
 *
 * This code serves as well as an example how to use the source code
 * generator of nsf to provide a C interface for optional nsf
 * packages.
 *
 * -gustaf neumann    March 27, 2011
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bson.h"
#include "mongo.h"
#include <gridfs.h>

#include <tcl.h>
#include <assert.h>
#include <nsf.h>

/*
 * Define the counters to generate nice symbols for pointer converter
 */
static int gridfsCount = 0;
static int gridfileCount = 0;
static int mongo_connectionCount = 0;

/***********************************************************************
 * The following definitions should not be here, but they are included
 * to get compilation going for the time being.
 ***********************************************************************/
typedef void *NsfObject;

#define PARSE_CONTEXT_PREALLOC 20
typedef struct {
  ClientData *clientData;
  int status;
  Tcl_Obj **objv;
  Tcl_Obj **full_objv;
  int *flags;
  ClientData clientData_static[PARSE_CONTEXT_PREALLOC];
  Tcl_Obj *objv_static[PARSE_CONTEXT_PREALLOC+1];
  int flags_static[PARSE_CONTEXT_PREALLOC+1];
  int lastobjc;
  int objc;
  int varArgs;
  NsfObject *object;
} ParseContext;

#define NSF_ARG_REQUIRED	0x000001

#define nr_elements(arr)  ((int) (sizeof(arr) / sizeof(arr[0])))
#define ObjStr(obj) (obj)->bytes ? (obj)->bytes : Tcl_GetString(obj)
#ifdef UNUSED
#elif defined(__GNUC__)
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
#else
# define UNUSED(x) x
#endif


#if defined(HAVE_STDINT_H)
# define HAVE_INTPTR_T 
# define HAVE_UINTPTR_T 
#endif

#if !defined(INT2PTR) && !defined(PTR2INT)
#   if defined(HAVE_INTPTR_T) || defined(intptr_t)
#	define INT2PTR(p) ((void *)(intptr_t)(p))
#	define PTR2INT(p) ((int)(intptr_t)(p))
#   else
#	define INT2PTR(p) ((void *)(p))
#	define PTR2INT(p) ((int)(p))
#   endif
#endif
#if !defined(UINT2PTR) && !defined(PTR2UINT)
#   if defined(HAVE_UINTPTR_T) || defined(uintptr_t)
#	define UINT2PTR(p) ((void *)(uintptr_t)(p))
#	define PTR2UINT(p) ((unsigned int)(uintptr_t)(p))
#   else
#	define UINT2PTR(p) ((void *)(p))
#	define PTR2UINT(p) ((unsigned int)(p))
#   endif
#endif


static int ArgumentParse(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], 
                         NsfObject *obj, Tcl_Obj *procName,
                         Nsf_Param *paramPtr, int nrParameters, int doCheck,
                         ParseContext *pc) {
  return Nsf_ArgumentParse(interp, objc, objv, (Nsf_Object *)obj, 
			   procName, paramPtr, nrParameters, 
			   doCheck, (Nsf_ParseContext *)pc);
}

/***********************************************************************
 * Include the generated mongo db api.
 ***********************************************************************/

#include "mongoAPI.h"

/***********************************************************************
 * Helper functions
 ***********************************************************************/

/*
 *----------------------------------------------------------------------
 *
 * BsonToList --
 *
 *      Convert a bson structure to a tagged list. Each value field is
 *      preceded by a tag denoting its bson type.
 *
 * Results:
 *      Tagged list.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
Tcl_Obj *
BsonToList(Tcl_Interp *interp, const char *data , int depth) {
  bson_iterator i;
  const char *key, *tag;
  char oidhex[25];
  Tcl_Obj *resultObj, *elemObj;

  bson_iterator_init( &i , data );
  resultObj = Tcl_NewListObj(0, NULL);

  while ( bson_iterator_next( &i ) ){
    bson_type t = bson_iterator_type( &i );

    if ( t == 0 )
      break;
    key = bson_iterator_key( &i );
    /*fprintf(stderr, "BsonToList: key %s t %d string %d\n", key, t, bson_string);*/

    switch ( t ){
    case bson_int:    tag = "integer"; elemObj = Tcl_NewIntObj(bson_iterator_int( &i )); break;
    case bson_long:   tag = "long";    elemObj = Tcl_NewLongObj(bson_iterator_long( &i )); break;
    case bson_date:   tag = "date";    elemObj = Tcl_NewLongObj(bson_iterator_date( &i )); break;
    case bson_double: tag = "double";  elemObj = Tcl_NewDoubleObj(bson_iterator_double( &i )); break;
    case bson_bool:   tag = "boolean"; elemObj = Tcl_NewBooleanObj(bson_iterator_bool( &i )); break;
    case bson_regex:  tag = "regex";   elemObj = Tcl_NewStringObj(bson_iterator_regex( &i ), -1); break;
    case bson_string: tag = "string";  elemObj = Tcl_NewStringObj(bson_iterator_string( &i ), -1); break;
    case bson_null:   tag = "null";    elemObj = Tcl_NewStringObj("null", 4); break;
    case bson_oid: {
      tag = "oid";
      bson_oid_to_string(bson_iterator_oid(&i), oidhex); 
      elemObj = Tcl_NewStringObj(oidhex, -1);
      break;
    }
    case bson_timestamp: {
      bson_timestamp_t ts;
      tag = "timestamp";
      ts = bson_iterator_timestamp( &i );
      elemObj = Tcl_NewListObj(0, NULL);
      Tcl_ListObjAppendElement(interp, elemObj, Tcl_NewIntObj(ts.t));
      Tcl_ListObjAppendElement(interp, elemObj, Tcl_NewIntObj(ts.i));
      break;
    }
    case bson_object: 
    case bson_array:
      tag = t == bson_object ? "object" : "array";
      elemObj = BsonToList(interp, bson_iterator_value( &i ) , depth + 1 );
      break;
    default:
      tag = "unknown";
      elemObj = Tcl_NewStringObj("", 0);
      NsfLog(interp, NSF_LOG_WARN, "BsonToList: unknown type %d", t);
    }

    Tcl_ListObjAppendElement(interp, resultObj, Tcl_NewStringObj(key, -1));
    Tcl_ListObjAppendElement(interp, resultObj, Tcl_NewStringObj(tag, -1));
    Tcl_ListObjAppendElement(interp, resultObj, elemObj);
  }

  return resultObj;
}

/*
 *----------------------------------------------------------------------
 *
 * BsonTagToType --
 *
 *      Convert a bson tag string to a bson_type. For the time being
 *      we compare as little as possible characters. In the future we
 *      might want to cache the bson tag in the Tcl_obj, maybe we can
 *      use Tcl_GetIndexFromObj();
 *
 * Results:
 *      bson_type.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
bson_type
BsonTagToType(Tcl_Interp *interp, char *tag) {
  char firstChar = *tag;
  
  switch (firstChar) {
  case 'a': /* array */   return bson_array;
  case 'b': /* bool */    return bson_bool;
  case 'd': 
    if (*(tag + 1) == 'a') /* date   */ return bson_date;
    if (*(tag + 1) == 'o') /* double */ return bson_double;
  case 'i': /* integer */ return bson_int;
  case 'l': /* long */    return bson_long;
  case 'n': /* null */    return bson_null;
  case 'o': 
    if  (*(tag + 1) == 'i') /* oid */ return bson_oid;
    if  (*(tag + 1) == 'b') /* object */ return bson_object;
    break;
  case 'r': /* regex */   return bson_regex;
  case 's': /* string */  return bson_string;
  case 't': /* timestamp */ return bson_timestamp;
  }

  NsfLog(interp, NSF_LOG_WARN, "BsonTagToType: Treat unknown tag '%s' as string", tag);
  return bson_string;
}

/*
 *----------------------------------------------------------------------
 *
 * BsonAppend --
 *
 *      append a tagged element to a bson buffer.
 *
 * Results:
 *      Tcl result code.
 *
 * Side effects:
 *      Value appended to bson buffer.
 *
 *----------------------------------------------------------------------
 */
static int
BsonAppend(Tcl_Interp *interp, bson_buffer *bbPtr, char *name, char *tag, Tcl_Obj *value) {
  int result = TCL_OK;
  bson_type t = BsonTagToType(interp, tag);

  /*fprintf(stderr, "BsonAppend: add name %s tag %s value '%s'\n", name, tag, ObjStr(value));*/

  switch ( t ){
  case bson_string: 
    bson_append_string(bbPtr, name, ObjStr(value)); 
    break;
  case bson_int: {
    int v;
    result = Tcl_GetIntFromObj(interp, value, &v);
    if (result != TCL_OK) break;
    bson_append_int(bbPtr, name, v);
    break;
  }
  case bson_double: {
    double v;
    result = Tcl_GetDoubleFromObj(interp, value, &v);
    if (result != TCL_OK) break;
    bson_append_double(bbPtr, name, v);
    break;
  }
  case bson_bool: {
    int v;
    result = Tcl_GetBooleanFromObj(interp, value, &v);
    if (result != TCL_OK) break;
    bson_append_bool(bbPtr, name, v);
    break;
  }
  case bson_long: {
    long v;
    result = Tcl_GetLongFromObj(interp, value, &v);
    if (result != TCL_OK) break;
    bson_append_long(bbPtr, name, v);
    break;
  }
  case bson_null: {
    bson_append_null(bbPtr, name);
    break;
  }
  case bson_oid: {
    bson_oid_t v;
    bson_oid_from_string(&v, ObjStr(value));
    bson_append_oid(bbPtr, name, &v);
    break;
  }
  case bson_regex: {
    char *opts = ""; /* TODO: how to handle regex opts? */
    bson_append_regex(bbPtr, name, ObjStr(value), opts);
    break;
  }
  case bson_date: {
    long v;
    result = Tcl_GetLongFromObj(interp, value, &v);
    if (result != TCL_OK) break;
    bson_append_date(bbPtr, name, v);
    break;
  }
  case bson_timestamp: {
    bson_timestamp_t v;
    Tcl_Obj **objv;
    int objc = 0;
    result = Tcl_ListObjGetElements(interp, value, &objc, &objv);
    if (result != TCL_OK || objc != 2) {
      return NsfPrintError(interp, "invalid timestamp: %s", ObjStr(value));
    }
    result = Tcl_GetIntFromObj(interp, objv[0], &v.t);
    if (result == TCL_OK) {
      result = Tcl_GetIntFromObj(interp, objv[1], &v.i);
    }
    if (result != TCL_OK) break;
    bson_append_timestamp(bbPtr, name, &v);
    break;
  }
  case bson_object: 
  case bson_array: {
    int i, objc;
    Tcl_Obj **objv;

    result = Tcl_ListObjGetElements(interp, value, &objc, &objv);
    if (result != TCL_OK || objc % 3 != 0) {
      return NsfPrintError(interp, "invalid %s value contain multiple of 3 elements %s", tag, ObjStr(value));
    }

    if (t == bson_object) {
      bson_append_start_object(bbPtr, name);
    } else {
      bson_append_start_array(bbPtr, name);
    }
    for (i = 0; i< objc; i += 3) {
      /*fprintf(stderr, "value %s, i %d, [0]: %s, [1]: %s, [2]: %s\n", ObjStr(value), i,
	ObjStr(objv[i]),  ObjStr(objv[i+1]), ObjStr(objv[i+2]));*/
      result = BsonAppend(interp, bbPtr, ObjStr(objv[i]),  ObjStr(objv[i+1]), objv[i+2]);
      if (result != TCL_OK) break;
    }
    
    /* 
     * finish_object works for arrays and objects 
     */
    bson_append_finish_object(bbPtr);
    break;
  }
    /* no default here, to get the warning to the compilation log for the time being */ 
  }
  return result;
}

/*
 *----------------------------------------------------------------------
 *
 * BsonAppendObjv --
 *
 *      Append all elements of objv to an uninitialized bson buffer.
 *
 * Results:
 *      Tcl result code.
 *
 * Side effects:
 *      Value appended to bson buffer.
 *
 *----------------------------------------------------------------------
 */
static int
BsonAppendObjv(Tcl_Interp *interp, bson *bPtr, int objc, Tcl_Obj **objv) {
  int i;
  bson_buffer buf[1];

  bson_buffer_init(buf);
  for (i = 0; i < objc; i += 3) {
    char *name = ObjStr(objv[i]);
    char *tag = ObjStr(objv[i+1]);
    Tcl_Obj *value = objv[i+2];
    /*fprintf(stderr, "adding pair '%s' (%s) '%s'\n", name, tag, ObjStr(value));*/
    BsonAppend(interp, buf, name, tag, value);
  }

  bson_from_buffer(bPtr, buf);
  
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NsfMongoGetHostPort --
 *
 *      Obtain from the provided string host and port. The provided
 *      string might be of the form "host" or "host:port". The parts
 *      are returned via arguments.
 *
 * Results:
 *      Tcl result code and variables bufferPtr, hostPtr and portPtr.
 *      If bufferPtr is not NULL, the caller must free it.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
/*
The entries of the list might be "host" (dns or ip
* addresses) or of the form "host:port".*/
static int
NsfMongoGetHostPort(CONST char *string, 
		    char **bufferPtr, char CONST**hostPtr, int *portPtr) {
  CONST char *colon, *host;
  int port;

  assert(string);
  colon = strchr(string, ':');

  if (colon) {
    /*
     * The passed string contained a colon; we must copy the entry,
     * since string is read only.
     */
    int length = strlen(string) + 1;
    int offset = colon-string;
    char *buffer;

    buffer = ckalloc(length);
    *bufferPtr = buffer;
    memcpy(buffer, string, length);
    buffer[offset] = '\0';
    host = buffer;
    port = atoi(buffer+offset+1);
  } else {
    /*
     * The passed string contained no colon.
     */
    *bufferPtr = NULL;
    host = string;
    port = 27017;
  }

  /*
   * Return always host and port via arguments.
   */
  *hostPtr = host;
  *portPtr = port;

  return TCL_OK;
}

/***********************************************************************
 * Define the api functions
 ***********************************************************************/

/*
cmd close NsfMongoClose {
  {-argName "conn" -required 1 -type mongo_connection -withObj 1}
}
*/
static int 
NsfMongoClose(Tcl_Interp *interp, mongo_connection *connPtr, Tcl_Obj *connObj) {

  if (connPtr) {
    mongo_destroy(connPtr);
    Nsf_PointerDelete(ObjStr(connObj), connPtr);
  }
  return TCL_OK;
}

/*
cmd connect NsfMongoConnect {
  {-argName "-replica-set" -required 0 -nrargs 1}
  {-argName "-server" -required 0 -nrargs 1 -type tclobj}
}
*/
static int 
NsfMongoConnect(Tcl_Interp *interp, CONST char *replicaSet, Tcl_Obj *server) {
  char channelName[80], *buffer = NULL;
  int result, port, objc = 0;
  mongo_connection *connPtr;
  mongo_conn_return status;
  Tcl_Obj **objv;
  CONST char *host;

  if (server) {
    result = Tcl_ListObjGetElements(interp, server, &objc, &objv);
    if (result != TCL_OK) {
      return NsfPrintError(interp, "The provided servers are not a well-formed list");
    }
  }

  connPtr = (mongo_connection *)ckalloc(sizeof(mongo_connection));

  if (objc == 0) {
    /*
     * No -server argument or an empty list was provided; use the
     * mongo default values.
     */
    status = mongo_connect( connPtr, "127.0.0.1", 27017 );

  } else if (objc == 1 && replicaSet == NULL) {
    /*
     * A single element was provided to -server, we have no replica
     * set specified.
     */
    NsfMongoGetHostPort(ObjStr(objv[0]), &buffer, &host, &port);
    status = mongo_connect( connPtr, host, port );
    if (buffer) {ckfree(buffer);}
    
  } else if (replicaSet) {
    /*
     * A list of 1 or more server was provided together with a replica
     * set. 
     */
    int i;

    mongo_replset_init_conn( connPtr, replicaSet);

    for (i = 0; i < objc; i++) {
      NsfMongoGetHostPort(ObjStr(objv[i]), &buffer, &host, &port);
      mongo_replset_add_seed(connPtr, host, port );
      if (buffer) {ckfree(buffer);}
    }

    status = mongo_replset_connect( connPtr );

  } else {
    ckfree((char *)connPtr);
    return NsfPrintError(interp, "A list of servers was provided, but not name for the replica set");
  }

  /*
   * Process the status from either mongo_connect() or
   * mongo_replset_connect().
   */
  if (status != mongo_conn_success) {
    char *errorMsg;

    ckfree((char *)connPtr);

    switch (status) {
    case mongo_conn_bad_arg:    errorMsg = "bad arguments"; break;
    case mongo_conn_no_socket:  errorMsg = "no socket"; break;
    case mongo_conn_fail:       errorMsg = "connection failed"; break;
    case mongo_conn_not_master: errorMsg = "not master"; break;
    case mongo_conn_bad_set_name: errorMsg = "replica set name doesn't match the existing replica set"; break;
    case mongo_conn_cannot_find_primary: errorMsg = "cannot find primary"; break;
    default: errorMsg = "unknown Error"; break;
    }

    return NsfPrintError(interp, errorMsg);
  }

  Nsf_PointerAdd(interp, channelName, "mongo_connection", connPtr);
  Tcl_SetObjResult(interp, Tcl_NewStringObj(channelName, -1));

  return TCL_OK;
}

/*
cmd query NsfMongoCount {
  {-argName "conn" -required 1 -type mongo_connection}
  {-argName "namespace" -required 1}
  {-argName "query" -required 1 -type tclobj}
}
*/
static int 
NsfMongoCount(Tcl_Interp *interp, mongo_connection *connPtr, CONST char *namespace, Tcl_Obj *queryObj) {
  int objc, result;
  Tcl_Obj **objv;
  char *db, *collection;
  int count, length;
  bson query[1];

  result = Tcl_ListObjGetElements(interp, queryObj, &objc, &objv);
  if (result != TCL_OK || (objc % 3 != 0)) {
    return NsfPrintError(interp, "%s: must contain a multiple of 3 elements", ObjStr(queryObj));
  }
  
  BsonAppendObjv(interp, query, objc, objv);
  
  length = strlen(namespace)+1;
  db = ckalloc(length);
  memcpy(db, namespace, length);
  collection = strchr(db, '.');

  if (collection != NULL) {
    /* successful */
    *collection = '\0';
    collection ++;
    count = mongo_count(connPtr, db, collection, query);
  } else {
    count = 0;
  }
       
  bson_destroy( query );
  ckfree(db);

  Tcl_SetObjResult(interp, Tcl_NewIntObj(count));

  return TCL_OK;
}


/*
cmd index NsfMongoIndex {
  {-argName "conn" -required 1 -type mongo_connection}
  {-argName "namespace" -required 1}
  {-argName "attributes" -required 1 -type tclobj}
  {-argName "-dropdups" -required 0 -nrargs 0}
  {-argName "-unique" -required 0 -nrargs 0}
}
*/
static int 
NsfMongoIndex(Tcl_Interp *interp, mongo_connection *connPtr, CONST char *namespace, Tcl_Obj *attributesObj, 
	      int withDropdups, int withUnique) {
  bson_bool_t success;
  int objc, result, options = 0;
  Tcl_Obj **objv;
  bson keys[1], out[1];

  result = Tcl_ListObjGetElements(interp, attributesObj, &objc, &objv);
  if (result != TCL_OK || (objc % 3 != 0)) {
    return NsfPrintError(interp, "%s: must contain a multiple of 3 elements", ObjStr(attributesObj));
  }
  BsonAppendObjv(interp, keys, objc, objv);
  if (withDropdups) {options |= MONGO_INDEX_DROP_DUPS;}
  if (withUnique) {options |= MONGO_INDEX_UNIQUE;}

  success = mongo_create_index(connPtr, namespace, keys, options, out);
  bson_destroy(keys);
  /* TODO: examples in mongo-client do not touch out; do we have to do
     something about it? */

  Tcl_SetObjResult(interp, Tcl_NewBooleanObj(success));
  return TCL_OK;
}



/*
cmd insert NsfMongoInsert {
  {-argName "conn" -required 1 -type mongo_connection}
  {-argName "namespace" -required 1}
  {-argName "values" -required 1 -type tclobj}
}
*/
static int NsfMongoInsert(Tcl_Interp *interp, mongo_connection *connPtr, CONST char *namespace, Tcl_Obj *valuesObj) {
  int i, objc, result;
  Tcl_Obj **objv, *resultObj;
  bson_buffer buf[1];
  bson b[1];

  result = Tcl_ListObjGetElements(interp, valuesObj, &objc, &objv);
  if (result != TCL_OK || (objc % 3 != 0)) {
    return NsfPrintError(interp, "%s: must contain a multiple of 3 elements", ObjStr(valuesObj));
  }
    
  bson_buffer_init(buf);
  bson_append_new_oid(buf, "_id");

  for (i = 0; i < objc; i += 3) {
    char *name = ObjStr(objv[i]);
    char *tag = ObjStr(objv[i+1]);
    Tcl_Obj *value = objv[i+2];
    /*fprintf(stderr, "adding pair '%s' (%s) '%s'\n", name, tag, ObjStr(value));*/
    BsonAppend(interp, buf, name, tag, value);
  }

  bson_from_buffer( b, buf );
  mongo_insert(connPtr, namespace, b);
  
  resultObj = BsonToList(interp, b->data, 0);
  Tcl_SetObjResult(interp, resultObj);

  bson_destroy(b);

  return TCL_OK;
}

/*
cmd query NsfMongoQuery {
  {-argName "conn" -required 1 -type mongo_connection}
  {-argName "namespace" -required 1}
  {-argName "query" -required 1 -type tclobj}
  {-argName "-atts" -required 0 -nrargs 1 -type tclobj}
  {-argName "-limit" -required 0 -type int32}
  {-argName "-skip" -required 0 -type int32}
}
*/
static int 
NsfMongoQuery(Tcl_Interp *interp, mongo_connection *connPtr, CONST char *namespace, 
	      Tcl_Obj *queryObj, Tcl_Obj *withAttsObj,
	      int withLimit, int withSkip) {
  int objc1, objc2, result;
  Tcl_Obj **objv1, **objv2, *resultObj;
  mongo_cursor *cursor;
  bson query[1];
  bson atts[1];

  /*fprintf(stderr, "NsfMongoQuery: namespace %s withLimit %d withSkip %d\n", 
    namespace, withLimit, withSkip);*/

  result = Tcl_ListObjGetElements(interp, queryObj, &objc1, &objv1);
  if (result != TCL_OK || (objc1 % 3 != 0)) {
    return NsfPrintError(interp, "%s: must contain a multiple of 3 elements", ObjStr(queryObj));
  }
  if (withAttsObj) {
    result = Tcl_ListObjGetElements(interp, withAttsObj, &objc2, &objv2);
    if (result != TCL_OK || (objc2 % 3 != 0)) {
      return NsfPrintError(interp, "%s: must contain a multiple of 3 elements", ObjStr(withAttsObj));
    }
  } else {
    objc2 = 0;
  }

  /* fprintf(stderr, "query # %d, atts # %d\n", objc1, objc2); */
  BsonAppendObjv(interp, query, objc1, objv1);
  BsonAppendObjv(interp, atts, objc2, objv2);

  resultObj = Tcl_NewListObj(0, NULL);

  /* 
   *  The last field of mongo_find is options, semantics are described here
   *  http://www.mongodb.org/display/DOCS/Mongo+Wire+Protocol#MongoWireProtocol-OPQUERY
   */
  cursor = mongo_find( connPtr, namespace, query, atts, withLimit, withSkip, 0 );
  while( mongo_cursor_next( cursor ) == MONGO_OK ) {
    Tcl_ListObjAppendElement(interp, resultObj, BsonToList(interp, (&cursor->current)->data, 0));
  }

  mongo_cursor_destroy( cursor );        
  bson_destroy( query );
  bson_destroy( atts );

  Tcl_SetObjResult(interp, resultObj);

  return TCL_OK;
}

/*
cmd remove NsfMongoRemove {
  {-argName "conn" -required 1 -type mongo_connection}
  {-argName "namespace" -required 1}
  {-argName "condition" -required 1 -type tclobj}
}
*/
static int 
NsfMongoRemove(Tcl_Interp *interp, mongo_connection *connPtr, CONST char *namespace, Tcl_Obj *conditionObj) {
  int objc, result;
  Tcl_Obj **objv;
  bson query[1];

  result = Tcl_ListObjGetElements(interp, conditionObj, &objc, &objv);
  if (result != TCL_OK || (objc % 3 != 0)) {
    return NsfPrintError(interp, "%s: must contain a multiple of 3 elements", ObjStr(conditionObj));
  }
 
  BsonAppendObjv(interp, query, objc, objv);
  mongo_remove(connPtr, namespace, query);

  bson_destroy(query);
  return TCL_OK;
}

/*
cmd insert NsfMongoUpdate {
  {-argName "conn" -required 1 -type mongo_connection}
  {-argName "namespace" -required 1}
  {-argName "cond" -required 1 -type tclobj}
  {-argName "values" -required 1 -type tclobj}
  {-argName "-upsert" -required 0 -nrargs 0}
  {-argName "-all" -required 0 -nrargs 0}
}
*/
static int 
NsfMongoUpdate(Tcl_Interp *interp, mongo_connection *connPtr, CONST char *namespace, 
	       Tcl_Obj *conditionObj, Tcl_Obj *valuesObj, int withUpsert, int withAll) {
  int objc, result, options = 0;
  Tcl_Obj **objv;
  bson cond[1], values[1];

  result = Tcl_ListObjGetElements(interp, conditionObj, &objc, &objv);
  if (result != TCL_OK || (objc % 3 != 0)) {
    return NsfPrintError(interp, "%s: must contain a multiple of 3 elements", ObjStr(conditionObj));
  }

  BsonAppendObjv(interp, cond, objc, objv);

  result = Tcl_ListObjGetElements(interp, valuesObj, &objc, &objv);
  if (result != TCL_OK || (objc % 3 != 0)) {
    bson_destroy(cond);
    return NsfPrintError(interp, "%s: must contain a multiple of 3 elements", ObjStr(valuesObj));
  }

  BsonAppendObjv(interp, values, objc, objv);

  if (withUpsert) {options |= 1;}
  if (withAll) {options |= 2;}
  mongo_update(connPtr, namespace, cond, values, options);
  
  return TCL_OK;
}

/***********************************************************************
 * GridFS interface
 ***********************************************************************/
/*
cmd gridfs::open NsfMongoGridFSOpen {
  {-argName "conn" -required 1 -type mongo_connection}
  {-argName "dbname" -required 1}
  {-argName "prefix" -required 1}
}
*/

static int
NsfMongoGridFSOpen(Tcl_Interp *interp, mongo_connection *connPtr, 
		   CONST char *dbname, CONST char *prefix) {
  char buffer[80];
  gridfs *gfsPtr;

  gfsPtr = (gridfs *)ckalloc(sizeof(gridfs));
  gridfs_init(connPtr, dbname, prefix, gfsPtr);

  Nsf_PointerAdd(interp, buffer, "gridfs", gfsPtr);
  Tcl_SetObjResult(interp, Tcl_NewStringObj(buffer, -1));

  return TCL_OK;
}


/*
cmd gridfs::remove_file NsfMongoGridFSRemoveFile {
  {-argName "gfs" -required 1 -type tclobj}
  {-argName "filename" -required 1}
}
*/
static int 
NsfMongoGridFSRemoveFile(Tcl_Interp *interp, gridfs *gridfsPtr, 
			CONST char *filename) {

  /* the current interfaces does not return a status ! */
  gridfs_remove_filename(gridfsPtr, filename);

  return TCL_OK;
}

/*
cmd gridfs::store_file NsfMongoGridFSStoreFile {
  {-argName "gfs" -required 1 -type gridfs}
  {-argName "filename" -required 1}
  {-argName "remotename" -required 1}
  {-argName "contenttype" -required 1}
}
*/
static int 
NsfMongoGridFSStoreFile(Tcl_Interp *interp, gridfs *gridfsPtr, 
			CONST char *filename, CONST char *remotename, 
			CONST char *contenttype) {
  bson b = gridfs_store_file(gridfsPtr, filename, remotename, contenttype);

  Tcl_SetObjResult(interp, BsonToList(interp, b.data, 0));
  
  return TCL_OK;
}

/*
cmd gridfs::close NsfMongoGridFSClose {
  {-argName "gfs" -required 1 -type gridfs -withObj 1}
}
*/
static int 
NsfMongoGridFSClose(Tcl_Interp *interp, gridfs *gridfsPtr, Tcl_Obj *gridfsObj) {

  gridfs_destroy(gridfsPtr);
  Nsf_PointerDelete(ObjStr(gridfsObj), gridfsPtr);

  return TCL_OK;
}

/***********************************************************************
 * GridFile interface
 ***********************************************************************/

/*
cmd gridfile::close NsfMongoGridFileClose {
  {-argName "file" -required 1 -type gridfile -withObj 1}
}
*/
static int 
NsfMongoGridFileClose(Tcl_Interp *interp, gridfile* gridFilePtr, Tcl_Obj *gridFileObj) {

  gridfile_destroy(gridFilePtr);
  Nsf_PointerDelete(ObjStr(gridFileObj), gridFilePtr);

  return TCL_OK;
}

/*
cmd gridfile::get_contentlength NsfMongoGridFileGetContentlength {
  {-argName "gridfile" -required 1 -type gridfile}
}
*/
static int 
NsfMongoGridFileGetContentlength(Tcl_Interp *interp, gridfile* gridFilePtr) {
  gridfs_offset len;

  len = gridfile_get_contentlength(gridFilePtr);
  Tcl_SetObjResult(interp, Tcl_NewLongObj(len));

  return TCL_OK;
}

/*
cmd gridfile::get_contenttype NsfMongoGridFileGetContentType {
  {-argName "gridfile" -required 1 -type gridfile}
}
*/
static int 
NsfMongoGridFileGetContentType(Tcl_Interp *interp, gridfile* gridFilePtr) {
  CONST char *contentType;

  contentType = gridfile_get_contenttype(gridFilePtr);
  Tcl_SetObjResult(interp, Tcl_NewStringObj(contentType, -1));

  return TCL_OK;
}

/*
cmd gridfile::get_metadata NsfMongoGridFileGetMetaData {
  {-argName "gridfile" -required 1 -type tclgridfile* gridFilePtrobj}
}
*/
static int 
NsfMongoGridFileGetMetaData(Tcl_Interp *interp, gridfile* gridFilePtr) {
  bson b;

  b = gridfile_get_metadata(gridFilePtr);
  Tcl_SetObjResult(interp, BsonToList(interp, b.data, 0));

  return TCL_OK;
}

/*
cmd gridfile::open NsfMongoGridFileOpen {
  {-argName "gfs" -required 1 -type gridfs}
  {-argName "filename" -required 1}
}
*/
static int 
NsfMongoGridFileOpen(Tcl_Interp *interp, gridfs *gridfsPtr, CONST char *filename) {
  char buffer[80];
  gridfile* gridFilePtr;
  int result;

  gridFilePtr = (gridfile *)ckalloc(sizeof(gridfile));
  result = gridfs_find_filename(gridfsPtr, filename, gridFilePtr);

  if (result == 1) {
    Nsf_PointerAdd(interp, buffer, "gridfile", gridFilePtr);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(buffer, -1));
  } else {
    ckfree((char *)gridFilePtr);
    Tcl_ResetResult(interp);
  }

  return TCL_OK;
}

/*
cmd gridfile::read NsfMongoGridFileRead {
  {-argName "gridfile" -required 1 -type gridfile}
  {-argName "size" -required 1 -type int}
}
*/
static int 
NsfMongoGridFileRead(Tcl_Interp *interp, gridfile *gridFilePtr, int size) {
  int readSize;
  char *buffer;

  buffer = ckalloc(size);
  readSize = gridfile_read(gridFilePtr, size, buffer);
  Tcl_SetObjResult(interp, Tcl_NewStringObj(buffer, readSize));
  ckfree(buffer);

  return TCL_OK;
}

/***********************************************************************
 * Finally, provide the necessary Tcl package interface.
 ***********************************************************************/

void 
Nsfmongo_Exit(ClientData clientData) {
  fprintf(stderr, "Nsfmongo Exit\n");
}

extern int 
Nsfmongo_Init(Tcl_Interp * interp) {
  int i;

#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL) {
        return TCL_ERROR;
    }

# ifdef USE_NSF_STUBS
    if (Nsf_InitStubs(interp, "1.1", 0) == NULL) {
        return TCL_ERROR;
    }
# endif
#else
    if (Tcl_PkgRequire(interp, "Tcl", TCL_VERSION, 0) == NULL) {
        return TCL_ERROR;
    }
#endif
    Tcl_PkgProvide(interp, "nsf::mongo", PACKAGE_VERSION);

#ifdef PACKAGE_REQUIRE_FROM_SLAVE_INTERP_WORKS_NOW
    if (Tcl_PkgRequire(interp, "nsf", XOTCLVERSION, 0) == NULL) {
        return TCL_ERROR;
    }
#endif

    Tcl_CreateExitHandler(Nsfmongo_Exit, interp);

    /*
     * register the pointer converter
     */
    Nsf_PointerTypeRegister(interp, "gridfs", &gridfsCount);
    Nsf_PointerTypeRegister(interp, "gridfile", &gridfileCount);
    Nsf_PointerTypeRegister(interp, "mongo_connection", &mongo_connectionCount);

    /* create all method commands (will use the namespaces above) */
    for (i=0; i < nr_elements(method_definitions)-1; i++) {
      Tcl_CreateObjCommand(interp, method_definitions[i].methodName, method_definitions[i].proc, 0, 0);
    }

    Tcl_SetIntObj(Tcl_GetObjResult(interp), 1);
    return TCL_OK;
}

extern int
Nsfmongo_SafeInit(interp)
    Tcl_Interp *interp;
{
    return Nsfmongo_Init(interp);
}
