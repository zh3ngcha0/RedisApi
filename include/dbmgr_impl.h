#ifndef __DATABASE_MGR_IMPL_H__
#define __DATABASE_MGR_IMPL_H__

#include <vnf_types.h>
#include <msg_api.h>

typedef struct {
    //Messaging API
    TaskID tid;
    MSGAPI msg_api;
}DBMgr;

#endif
