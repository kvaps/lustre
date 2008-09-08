/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * GPL HEADER START
 *
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License version 2 for more details (a copy is included
 * in the LICENSE file that accompanied this code).
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; If not, see
 * http://www.sun.com/software/products/lustre/docs/GPLv2.pdf
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 *
 * GPL HEADER END
 */
/*
 * Copyright  2008 Sun Microsystems, Inc. All rights reserved
 * Use is subject to license terms.
 */
/*
 * This file is part of Lustre, http://www.lustre.org/
 * Lustre is a trademark of Sun Microsystems, Inc.
 */

#ifndef _LUSTRE_NET_H
#define _LUSTRE_NET_H

#if defined(__linux__)
#include <linux/lustre_net.h>
#elif defined(__APPLE__)
#include <darwin/lustre_net.h>
#elif defined(__WINNT__)
#include <winnt/lustre_net.h>
#else
#error Unsupported operating system.
#endif

#include <libcfs/libcfs.h>
// #include <obd.h>
#include <lnet/lnet.h>
#include <lustre/lustre_idl.h>
#include <lustre_ha.h>
#include <lustre_sec.h>
#include <lustre_import.h>
#include <lprocfs_status.h>
#include <lu_object.h>
#include <lustre_req_layout.h>

/* MD flags we _always_ use */
#define PTLRPC_MD_OPTIONS  0

/* Define maxima for bulk I/O
 * CAVEAT EMPTOR, with multinet (i.e. routers forwarding between networks)
 * these limits are system wide and not interface-local. */
#define PTLRPC_MAX_BRW_BITS     LNET_MTU_BITS
#define PTLRPC_MAX_BRW_SIZE     (1<<LNET_MTU_BITS)
#define PTLRPC_MAX_BRW_PAGES    (PTLRPC_MAX_BRW_SIZE >> CFS_PAGE_SHIFT)

/* When PAGE_SIZE is a constant, we can check our arithmetic here with cpp! */
#ifdef __KERNEL__
# if ((PTLRPC_MAX_BRW_PAGES & (PTLRPC_MAX_BRW_PAGES - 1)) != 0)
#  error "PTLRPC_MAX_BRW_PAGES isn't a power of two"
# endif
# if (PTLRPC_MAX_BRW_SIZE != (PTLRPC_MAX_BRW_PAGES * CFS_PAGE_SIZE))
#  error "PTLRPC_MAX_BRW_SIZE isn't PTLRPC_MAX_BRW_PAGES * CFS_PAGE_SIZE"
# endif
# if (PTLRPC_MAX_BRW_SIZE > LNET_MTU)
#  error "PTLRPC_MAX_BRW_SIZE too big"
# endif
# if (PTLRPC_MAX_BRW_PAGES > LNET_MAX_IOV)
#  error "PTLRPC_MAX_BRW_PAGES too big"
# endif
#endif /* __KERNEL__ */

/* Size over which to OBD_VMALLOC() rather than OBD_ALLOC() service request
 * buffers */
#define SVC_BUF_VMALLOC_THRESHOLD (2 * CFS_PAGE_SIZE)

/* The following constants determine how memory is used to buffer incoming
 * service requests.
 *
 * ?_NBUFS              # buffers to allocate when growing the pool
 * ?_BUFSIZE            # bytes in a single request buffer
 * ?_MAXREQSIZE         # maximum request service will receive
 *
 * When fewer than ?_NBUFS/2 buffers are posted for receive, another chunk
 * of ?_NBUFS is added to the pool.
 *
 * Messages larger than ?_MAXREQSIZE are dropped.  Request buffers are
 * considered full when less than ?_MAXREQSIZE is left in them.
 */

#define LDLM_THREADS_AUTO_MIN                                                 \
        min((int)(num_online_cpus() * num_online_cpus() * 2), 8)
#define LDLM_THREADS_AUTO_MAX (LDLM_THREADS_AUTO_MIN * 16)
#define LDLM_BL_THREADS  LDLM_THREADS_AUTO_MIN
#define LDLM_NBUFS      (64 * num_online_cpus())
#define LDLM_BUFSIZE    (8 * 1024)
#define LDLM_MAXREQSIZE (5 * 1024)
#define LDLM_MAXREPSIZE (1024)

#define MDT_MIN_THREADS 2UL
#define MDT_MAX_THREADS 512UL
#define MDT_NUM_THREADS max(min_t(unsigned long, MDT_MAX_THREADS, \
                                  num_physpages >> (25 - CFS_PAGE_SHIFT)), 2UL)
#define FLD_NUM_THREADS max(min_t(unsigned long, MDT_MAX_THREADS, \
                                  num_physpages >> (25 - CFS_PAGE_SHIFT)), 2UL)
#define SEQ_NUM_THREADS max(min_t(unsigned long, MDT_MAX_THREADS, \
                                  num_physpages >> (25 - CFS_PAGE_SHIFT)), 2UL)

/* Absolute limits */
#define MDS_THREADS_MIN 2
#define MDS_THREADS_MAX 512
#define MDS_THREADS_MIN_READPAGE 2
#define MDS_NBUFS       (64 * num_online_cpus())
#define MDS_BUFSIZE     (8 * 1024)
/* Assume file name length = FNAME_MAX = 256 (true for ext3).
 *        path name length = PATH_MAX = 4096
 *        LOV MD size max  = EA_MAX = 4000
 * symlink:  FNAME_MAX + PATH_MAX  <- largest
 * link:     FNAME_MAX + PATH_MAX  (mds_rec_link < mds_rec_create)
 * rename:   FNAME_MAX + FNAME_MAX
 * open:     FNAME_MAX + EA_MAX
 *
 * MDS_MAXREQSIZE ~= 4736 bytes =
 * lustre_msg + ldlm_request + mds_body + mds_rec_create + FNAME_MAX + PATH_MAX
 * MDS_MAXREPSIZE ~= 8300 bytes = lustre_msg + llog_header
 * or, for mds_close() and mds_reint_unlink() on a many-OST filesystem:
 *      = 9210 bytes = lustre_msg + mds_body + 160 * (easize + cookiesize)
 *
 * Realistic size is about 512 bytes (20 character name + 128 char symlink),
 * except in the open case where there are a large number of OSTs in a LOV.
 */
#define MDS_MAXREQSIZE  (5 * 1024)
#define MDS_MAXREPSIZE  max(9 * 1024, 280 + LOV_MAX_STRIPE_COUNT * 56)

/* FLD_MAXREQSIZE == lustre_msg + __u32 padding + ptlrpc_body + opc + md_fld */
#define FLD_MAXREQSIZE  (160)

/* FLD_MAXREPSIZE == lustre_msg + ptlrpc_body + md_fld */
#define FLD_MAXREPSIZE  (152)

/* SEQ_MAXREQSIZE == lustre_msg + __u32 padding + ptlrpc_body + opc + lu_range +
 * __u32 padding */
#define SEQ_MAXREQSIZE  (160)

/* SEQ_MAXREPSIZE == lustre_msg + ptlrpc_body + lu_range */
#define SEQ_MAXREPSIZE  (152)

#define MGS_THREADS_AUTO_MIN 2
#define MGS_THREADS_AUTO_MAX 32
#define MGS_NBUFS       (64 * num_online_cpus())
#define MGS_BUFSIZE     (8 * 1024)
#define MGS_MAXREQSIZE  (7 * 1024)
#define MGS_MAXREPSIZE  (9 * 1024)

/* Absolute limits */
#define OSS_THREADS_MIN 2
#define OSS_THREADS_MAX 512
#define OST_NBUFS       (64 * num_online_cpus())
#define OST_BUFSIZE     (8 * 1024)
/* OST_MAXREQSIZE ~= 4768 bytes =
 * lustre_msg + obdo + 16 * obd_ioobj + 256 * niobuf_remote
 *
 * - single object with 16 pages is 512 bytes
 * - OST_MAXREQSIZE must be at least 1 page of cookies plus some spillover
 */
#define OST_MAXREQSIZE  (5 * 1024)
#define OST_MAXREPSIZE  (9 * 1024)

/* Macro to hide a typecast. */
#define ptlrpc_req_async_args(req) ((void *)&req->rq_async_args)

struct ptlrpc_connection {
        struct hlist_node       c_hash;
        lnet_nid_t              c_self;
        lnet_process_id_t       c_peer;
        struct obd_uuid         c_remote_uuid;
        atomic_t                c_refcount;
};

struct ptlrpc_client {
        __u32                   cli_request_portal;
        __u32                   cli_reply_portal;
        char                   *cli_name;
};

/* state flags of requests */
/* XXX only ones left are those used by the bulk descs as well! */
#define PTL_RPC_FL_INTR      (1 << 0)  /* reply wait was interrupted by user */
#define PTL_RPC_FL_TIMEOUT   (1 << 7)  /* request timed out waiting for reply */

#define REQ_MAX_ACK_LOCKS 8

union ptlrpc_async_args {
        /* Scratchpad for passing args to completion interpreter. Users
         * cast to the struct of their choosing, and LASSERT that this is
         * big enough.  For _tons_ of context, OBD_ALLOC a struct and store
         * a pointer to it here.  The pointer_arg ensures this struct is at
         * least big enough for that. */
        void      *pointer_arg[9];
        __u64      space[4];
};

struct ptlrpc_request_set;
typedef int (*set_interpreter_func)(struct ptlrpc_request_set *, void *, int);

struct ptlrpc_request_set {
        int               set_remaining; /* # uncompleted requests */
        cfs_waitq_t       set_waitq;
        cfs_waitq_t      *set_wakeup_ptr;
        struct list_head  set_requests;
        struct list_head  set_cblist; /* list of completion callbacks */
        set_interpreter_func    set_interpret; /* completion callback */
        void              *set_arg; /* completion context */
        /* locked so that any old caller can communicate requests to
         * the set holder who can then fold them into the lock-free set */
        spinlock_t        set_new_req_lock;
        struct list_head  set_new_requests;
};

struct ptlrpc_set_cbdata {
        struct list_head        psc_item;
        set_interpreter_func    psc_interpret;
        void                   *psc_data;
};

struct ptlrpc_bulk_desc;

/*
 * ptlrpc callback & work item stuff
 */
struct ptlrpc_cb_id {
        void   (*cbid_fn)(lnet_event_t *ev);     /* specific callback fn */
        void    *cbid_arg;                      /* additional arg */
};

#define RS_MAX_LOCKS 8
#define RS_DEBUG     1

struct ptlrpc_reply_state {
        struct ptlrpc_cb_id    rs_cb_id;
        struct list_head       rs_list;
        struct list_head       rs_exp_list;
        struct list_head       rs_obd_list;
#if RS_DEBUG
        struct list_head       rs_debug_list;
#endif
        /* updates to following flag serialised by srv_request_lock */
        unsigned long          rs_difficult:1;     /* ACK/commit stuff */
        unsigned long          rs_scheduled:1;     /* being handled? */
        unsigned long          rs_scheduled_ever:1;/* any schedule attempts? */
        unsigned long          rs_handled:1;  /* been handled yet? */
        unsigned long          rs_on_net:1;   /* reply_out_callback pending? */
        unsigned long          rs_prealloc:1; /* rs from prealloc list */

        int                    rs_size;
        __u64                  rs_transno;
        __u64                  rs_xid;
        struct obd_export     *rs_export;
        struct ptlrpc_service *rs_service;
        lnet_handle_md_t       rs_md_h;
        atomic_t               rs_refcount;

        struct ptlrpc_svc_ctx *rs_svc_ctx;
        struct lustre_msg     *rs_repbuf;       /* wrapper */
        int                    rs_repbuf_len;   /* wrapper buf length */
        int                    rs_repdata_len;  /* wrapper msg length */
        struct lustre_msg     *rs_msg;          /* reply message */

        /* locks awaiting client reply ACK */
        int                    rs_nlocks;
        struct lustre_handle   rs_locks[RS_MAX_LOCKS];
        ldlm_mode_t            rs_modes[RS_MAX_LOCKS];
};

struct ptlrpc_thread;

enum rq_phase {
        RQ_PHASE_NEW         = 0xebc0de00,
        RQ_PHASE_RPC         = 0xebc0de01,
        RQ_PHASE_BULK        = 0xebc0de02,
        RQ_PHASE_INTERPRET   = 0xebc0de03,
        RQ_PHASE_COMPLETE    = 0xebc0de04,
};

struct ptlrpc_request_pool {
        spinlock_t prp_lock;
        struct list_head prp_req_list;    /* list of ptlrpc_request structs */
        int prp_rq_size;
        void (*prp_populate)(struct ptlrpc_request_pool *, int);
};

struct lu_context;
struct lu_env;

struct ptlrpc_request {
        int rq_type; /* one of PTL_RPC_MSG_* */
        struct list_head rq_list;
        struct list_head rq_timed_list;         /* server-side early replies */
        struct list_head rq_history_list;       /* server-side history */
        __u64            rq_history_seq;        /* history sequence # */
        int rq_status;
        spinlock_t rq_lock;
        /* client-side flags are serialized by rq_lock */
        unsigned long rq_intr:1, rq_replied:1, rq_err:1,
                rq_timedout:1, rq_resend:1, rq_restart:1,
                /*
                 * when ->rq_replay is set, request is kept by the client even
                 * after server commits corresponding transaction. This is
                 * used for operations that require sequence of multiple
                 * requests to be replayed. The only example currently is file
                 * open/close/dw/setattr. When last request in such a sequence
                 * is committed, ->rq_replay is cleared on all requests in the
                 * sequence.
                 */
                rq_replay:1,
                /* this is the last request in the sequence. */
                rq_sequence:1,
                rq_no_resend:1, rq_waiting:1, rq_receiving_reply:1,
                rq_no_delay:1, rq_net_err:1, rq_wait_ctx:1,
                rq_early:1, rq_must_unlink:1,
                /* server-side flags */
                rq_packed_final:1,  /* packed final reply */
                rq_sent_final:1;    /* stop sending early replies */

        enum rq_phase rq_phase; /* one of RQ_PHASE_* */
        atomic_t rq_refcount;   /* client-side refcount for SENT race,
                                   server-side refcounf for multiple replies */

        struct ptlrpc_thread *rq_svc_thread; /* initial thread servicing req */

        int rq_request_portal;  /* XXX FIXME bug 249 */
        int rq_reply_portal;    /* XXX FIXME bug 249 */

        int rq_nob_received; /* client-side # reply bytes actually received  */

        int rq_reqlen;
        struct lustre_msg *rq_reqmsg;

        int rq_replen;
        struct lustre_msg *rq_repmsg;
        __u64 rq_transno;
        __u64 rq_xid;
        struct list_head rq_replay_list;
        struct list_head rq_mod_list;

        struct ptlrpc_cli_ctx   *rq_cli_ctx;     /* client's half ctx */
        struct ptlrpc_svc_ctx   *rq_svc_ctx;     /* server's half ctx */
        struct list_head         rq_ctx_chain;   /* link to waited ctx */

        struct sptlrpc_flavor    rq_flvr;        /* client & server */
        enum lustre_sec_part     rq_sp_from;

        unsigned long            /* client/server security flags */
                                 rq_ctx_init:1,      /* context initiation */
                                 rq_ctx_fini:1,      /* context destroy */
                                 rq_bulk_read:1,     /* request bulk read */
                                 rq_bulk_write:1,    /* request bulk write */
                                 /* server authentication flags */
                                 rq_auth_gss:1,      /* authenticated by gss */
                                 rq_auth_remote:1,   /* authed as remote user */
                                 rq_auth_usr_root:1, /* authed as root */
                                 rq_auth_usr_mdt:1,  /* authed as mdt */
                                 /* security tfm flags */
                                 rq_pack_udesc:1,
                                 rq_pack_bulk:1,
                                 /* doesn't expect reply FIXME */
                                 rq_no_reply:1;

        uid_t                    rq_auth_uid;        /* authed uid */
        uid_t                    rq_auth_mapped_uid; /* authed uid mapped to */

        /* (server side), pointed directly into req buffer */
        struct ptlrpc_user_desc *rq_user_desc;

        /* early replies go to offset 0, regular replies go after that */
        unsigned int             rq_reply_off;

        /* various buffer pointers */
        struct lustre_msg       *rq_reqbuf;      /* req wrapper */
        int                      rq_reqbuf_len;  /* req wrapper buf len */
        int                      rq_reqdata_len; /* req wrapper msg len */
        char                    *rq_repbuf;      /* rep buffer */
        int                      rq_repbuf_len;  /* rep buffer len */
        struct lustre_msg       *rq_repdata;     /* rep wrapper msg */
        int                      rq_repdata_len; /* rep wrapper msg len */
        struct lustre_msg       *rq_clrbuf;      /* only in priv mode */
        int                      rq_clrbuf_len;  /* only in priv mode */
        int                      rq_clrdata_len; /* only in priv mode */

        __u32 rq_req_swab_mask;
        __u32 rq_rep_swab_mask;

        int rq_import_generation;
        enum lustre_imp_state rq_send_state;

        int rq_early_count;           /* how many early replies (for stats) */

        /* client+server request */
        lnet_handle_md_t     rq_req_md_h;
        struct ptlrpc_cb_id  rq_req_cbid;

        /* server-side... */
        struct timeval       rq_arrival_time;       /* request arrival time */
        struct ptlrpc_reply_state *rq_reply_state;  /* separated reply state */
        struct ptlrpc_request_buffer_desc *rq_rqbd; /* incoming request buffer*/
#ifdef CRAY_XT3
        __u32                rq_uid;            /* peer uid, used in MDS only */
#endif

        /* client-only incoming reply */
        lnet_handle_md_t     rq_reply_md_h;
        cfs_waitq_t          rq_reply_waitq;
        struct ptlrpc_cb_id  rq_reply_cbid;

        lnet_nid_t           rq_self;
        lnet_process_id_t    rq_peer;
        struct obd_export   *rq_export;
        struct obd_import   *rq_import;

        void (*rq_replay_cb)(struct ptlrpc_request *);
        void (*rq_commit_cb)(struct ptlrpc_request *);
        void  *rq_cb_data;

        struct ptlrpc_bulk_desc *rq_bulk;/* client side bulk */

        /* client outgoing req */
        time_t rq_sent;                  /* when request/reply sent (secs), or
                                          * time when request should be sent */

        volatile time_t rq_deadline;     /* when request must finish. volatile
               so that servers' early reply updates to the deadline aren't
               kept in per-cpu cache */
        int    rq_timeout;               /* service time estimate (secs) */

        /* Multi-rpc bits */
        struct list_head rq_set_chain;
        struct ptlrpc_request_set *rq_set;
        void *rq_interpret_reply;               /* Async completion handler */
        union ptlrpc_async_args rq_async_args;  /* Async completion context */
        struct ptlrpc_request_pool *rq_pool;    /* Pool if request from
                                                   preallocated list */
        struct lu_context           rq_session;

        /* request format */
        struct req_capsule          rq_pill;
};

static inline void ptlrpc_close_replay_seq(struct ptlrpc_request *req)
{
        spin_lock(&req->rq_lock);
        req->rq_replay = 0;
        req->rq_sequence = 1;
        spin_unlock(&req->rq_lock);
}

static inline void lustre_set_req_swabbed(struct ptlrpc_request *req, int index)
{
        LASSERT(index < sizeof(req->rq_req_swab_mask) * 8);
        LASSERT((req->rq_req_swab_mask & (1 << index)) == 0);
        req->rq_req_swab_mask |= 1 << index;
}

static inline void lustre_set_rep_swabbed(struct ptlrpc_request *req, int index)
{
        LASSERT(index < sizeof(req->rq_rep_swab_mask) * 8);
        LASSERT((req->rq_rep_swab_mask & (1 << index)) == 0);
        req->rq_rep_swab_mask |= 1 << index;
}

static inline int lustre_req_swabbed(struct ptlrpc_request *req, int index)
{
        LASSERT(index < sizeof(req->rq_req_swab_mask) * 8);
        return req->rq_req_swab_mask & (1 << index);
}

static inline int lustre_rep_swabbed(struct ptlrpc_request *req, int index)
{
        LASSERT(index < sizeof(req->rq_rep_swab_mask) * 8);
        return req->rq_rep_swab_mask & (1 << index);
}

static inline const char *
ptlrpc_rqphase2str(const struct ptlrpc_request *req)
{
        switch (req->rq_phase) {
        case RQ_PHASE_NEW:
                return "New";
        case RQ_PHASE_RPC:
                return "Rpc";
        case RQ_PHASE_BULK:
                return "Bulk";
        case RQ_PHASE_INTERPRET:
                return "Interpret";
        case RQ_PHASE_COMPLETE:
                return "Complete";
        default:
                return "?Phase?";
        }
}

/* Spare the preprocessor, spoil the bugs. */
#define FLAG(field, str) (field ? str : "")

#define DEBUG_REQ_FLAGS(req)                                                    \
        ptlrpc_rqphase2str(req),                                                \
        FLAG(req->rq_intr, "I"), FLAG(req->rq_replied, "R"),                    \
        FLAG(req->rq_err, "E"),                                                 \
        FLAG(req->rq_timedout, "X") /* eXpired */, FLAG(req->rq_resend, "S"),   \
        FLAG(req->rq_restart, "T"), FLAG(req->rq_replay, "P"),                  \
        FLAG(req->rq_no_resend, "N"),                                           \
        FLAG(req->rq_waiting, "W"),                                             \
        FLAG(req->rq_wait_ctx, "C")

#define REQ_FLAGS_FMT "%s:%s%s%s%s%s%s%s%s%s%s"

void _debug_req(struct ptlrpc_request *req, __u32 mask,
                struct libcfs_debug_msg_data *data, const char *fmt, ...)
        __attribute__ ((format (printf, 4, 5)));

#define debug_req(cdls, level, req, file, func, line, fmt, a...)              \
do {                                                                          \
        CHECK_STACK();                                                        \
                                                                              \
        if (((level) & D_CANTMASK) != 0 ||                                    \
            ((libcfs_debug & (level)) != 0 &&                                 \
             (libcfs_subsystem_debug & DEBUG_SUBSYSTEM) != 0)) {              \
                static struct libcfs_debug_msg_data _req_dbg_data =           \
                DEBUG_MSG_DATA_INIT(cdls, DEBUG_SUBSYSTEM, file, func, line); \
                _debug_req((req), (level), &_req_dbg_data, fmt, ##a);         \
        }                                                                     \
} while(0)

/* for most callers (level is a constant) this is resolved at compile time */
#define DEBUG_REQ(level, req, fmt, args...)                                   \
do {                                                                          \
        if ((level) & (D_ERROR | D_WARNING)) {                                \
                static cfs_debug_limit_state_t cdls;                          \
                debug_req(&cdls, level, req, __FILE__, __func__, __LINE__,    \
                          "@@@ "fmt" ", ## args);                             \
        } else                                                                \
                debug_req(NULL, level, req, __FILE__, __func__, __LINE__,     \
                          "@@@ "fmt" ", ## args);                             \
} while (0)

struct ptlrpc_bulk_page {
        struct list_head bp_link;
        int              bp_buflen;
        int              bp_pageoffset; /* offset within a page */
        struct page     *bp_page;
};

#define BULK_GET_SOURCE   0
#define BULK_PUT_SINK     1
#define BULK_GET_SINK     2
#define BULK_PUT_SOURCE   3

struct ptlrpc_bulk_desc {
        unsigned long bd_success:1;              /* completed successfully */
        unsigned long bd_network_rw:1;           /* accessible to the network */
        unsigned long bd_type:2;                 /* {put,get}{source,sink} */
        unsigned long bd_registered:1;           /* client side */
        spinlock_t   bd_lock;                   /* serialise with callback */
        int bd_import_generation;
        struct obd_export *bd_export;
        struct obd_import *bd_import;
        __u32 bd_portal;
        struct ptlrpc_request *bd_req;          /* associated request */
        cfs_waitq_t            bd_waitq;        /* server side only WQ */
        int                    bd_iov_count;    /* # entries in bd_iov */
        int                    bd_max_iov;      /* allocated size of bd_iov */
        int                    bd_nob;          /* # bytes covered */
        int                    bd_nob_transferred; /* # bytes GOT/PUT */

        __u64                  bd_last_xid;

        struct ptlrpc_cb_id    bd_cbid;         /* network callback info */
        lnet_handle_md_t       bd_md_h;         /* associated MD */
        lnet_nid_t             bd_sender;       /* stash event::sender */

        cfs_page_t           **bd_enc_pages;
#if defined(__KERNEL__)
        lnet_kiov_t            bd_iov[0];
#else
        lnet_md_iovec_t        bd_iov[0];
#endif
};

struct ptlrpc_thread {

        struct list_head t_link; /* active threads in svc->srv_threads */

        void *t_data;            /* thread-private data (preallocated memory) */
        __u32 t_flags;

        unsigned int t_id; /* service thread index, from ptlrpc_start_threads */
        cfs_waitq_t t_ctl_waitq;
        struct lu_env *t_env;
};

struct ptlrpc_request_buffer_desc {
        struct list_head       rqbd_list;
        struct list_head       rqbd_reqs;
        struct ptlrpc_service *rqbd_service;
        lnet_handle_md_t       rqbd_md_h;
        int                    rqbd_refcount;
        char                  *rqbd_buffer;
        struct ptlrpc_cb_id    rqbd_cbid;
        struct ptlrpc_request  rqbd_req;
};

typedef int (*svc_handler_t)(struct ptlrpc_request *req);
typedef void (*svcreq_printfn_t)(void *, struct ptlrpc_request *);

struct ptlrpc_service {
        struct list_head srv_list;              /* chain thru all services */
        int              srv_max_req_size;      /* biggest request to receive */
        int              srv_max_reply_size;    /* biggest reply to send */
        int              srv_buf_size;          /* size of individual buffers */
        int              srv_nbuf_per_group;    /* # buffers to allocate in 1 group */
        int              srv_nbufs;             /* total # req buffer descs allocated */
        int              srv_threads_min;       /* threads to start at SOW */
        int              srv_threads_max;       /* thread upper limit */
        int              srv_threads_started;   /* index of last started thread */
        int              srv_threads_running;   /* # running threads */
        int              srv_n_difficult_replies; /* # 'difficult' replies */
        int              srv_n_active_reqs;     /* # reqs being served */
        cfs_duration_t   srv_rqbd_timeout;      /* timeout before re-posting reqs, in tick */
        int              srv_watchdog_factor;   /* soft watchdog timeout mutiplier */
        unsigned         srv_cpu_affinity:1;    /* bind threads to CPUs */
        unsigned         srv_at_check:1;        /* check early replies */
        cfs_time_t       srv_at_checktime;      /* debug */

        __u32            srv_req_portal;
        __u32            srv_rep_portal;

        /* AT stuff */
        struct adaptive_timeout srv_at_estimate;/* estimated rpc service time */
        spinlock_t        srv_at_lock;
        struct list_head  srv_at_list;          /* reqs waiting for replies */
        cfs_timer_t       srv_at_timer;         /* early reply timer */

        int               srv_n_queued_reqs;    /* # reqs in either of the queues below */
        struct list_head  srv_req_in_queue;     /* incoming reqs */
        struct list_head  srv_request_queue;    /* reqs waiting for service */

        struct list_head  srv_request_history;  /* request history */
        __u64             srv_request_seq;      /* next request sequence # */
        __u64             srv_request_max_cull_seq; /* highest seq culled from history */
        svcreq_printfn_t  srv_request_history_print_fn; /* service-specific print fn */

        struct list_head  srv_idle_rqbds;       /* request buffers to be reposted */
        struct list_head  srv_active_rqbds;     /* req buffers receiving */
        struct list_head  srv_history_rqbds;    /* request buffer history */
        int               srv_nrqbd_receiving;  /* # posted request buffers */
        int               srv_n_history_rqbds;  /* # request buffers in history */
        int               srv_max_history_rqbds;/* max # request buffers in history */

        atomic_t          srv_outstanding_replies;
        struct list_head  srv_active_replies;   /* all the active replies */
        struct list_head  srv_reply_queue;      /* replies waiting for service */

        cfs_waitq_t       srv_waitq; /* all threads sleep on this. This
                                      * wait-queue is signalled when new
                                      * incoming request arrives and when
                                      * difficult reply has to be handled. */

        struct list_head   srv_threads;         /* service thread list */
        svc_handler_t      srv_handler;

        char *srv_name; /* only statically allocated strings here; we don't clean them */
        char *srv_thread_name; /* only statically allocated strings here; we don't clean them */

        spinlock_t               srv_lock;

        cfs_proc_dir_entry_t    *srv_procroot;
        struct lprocfs_stats    *srv_stats;

        /* List of free reply_states */
        struct list_head         srv_free_rs_list;
        /* waitq to run, when adding stuff to srv_free_rs_list */
        cfs_waitq_t              srv_free_rs_waitq;

        /*
         * Tags for lu_context associated with this thread, see struct
         * lu_context.
         */
        __u32                    srv_ctx_tags;
        /*
         * if non-NULL called during thread creation (ptlrpc_start_thread())
         * to initialize service specific per-thread state.
         */
        int (*srv_init)(struct ptlrpc_thread *thread);
        /*
         * if non-NULL called during thread shutdown (ptlrpc_main()) to
         * destruct state created by ->srv_init().
         */
        void (*srv_done)(struct ptlrpc_thread *thread);

        //struct ptlrpc_srv_ni srv_interfaces[0];
};

struct ptlrpcd_ctl {
        /**
         * Ptlrpc thread control flags (LIOD_START, LIOD_STOP, LIOD_STOP_FORCE)
         */
        unsigned long               pc_flags;
        /**
         * Thread lock protecting structure fields.
         */
        spinlock_t                  pc_lock;
        /**
         * Start completion.
         */
        struct completion           pc_starting;
        /**
         * Stop completion.
         */
        struct completion           pc_finishing;
        /**
         * Thread requests set.
         */
        struct ptlrpc_request_set  *pc_set;
        /**
         * Thread name used in cfs_daemonize()
         */
        char                        pc_name[16];
#ifndef __KERNEL__
        /**
         * Async rpcs flag to make sure that ptlrpcd_check() is called only
         * once.
         */
        int                         pc_recurred;
        /**
         * Currently not used.
         */
        void                       *pc_callback;
        /**
         * User-space async rpcs callback.
         */
        void                       *pc_wait_callback;
        /**
         * User-space check idle rpcs callback.
         */
        void                       *pc_idle_callback;
#endif
};

/* Bits for pc_flags */
enum ptlrpcd_ctl_flags {
        /**
         * Ptlrpc thread start flag.
         */
        LIOD_START       = 1 << 0,
        /**
         * Ptlrpc thread stop flag.
         */
        LIOD_STOP        = 1 << 1,
        /**
         * Ptlrpc thread stop force flag. This will cause also
         * aborting any inflight rpcs handled by thread.
         */
        LIOD_STOP_FORCE  = 1 << 2
};

/* ptlrpc/events.c */
extern lnet_handle_eq_t ptlrpc_eq_h;
extern int ptlrpc_uuid_to_peer(struct obd_uuid *uuid,
                               lnet_process_id_t *peer, lnet_nid_t *self);
extern void request_out_callback (lnet_event_t *ev);
extern void reply_in_callback(lnet_event_t *ev);
extern void client_bulk_callback (lnet_event_t *ev);
extern void request_in_callback(lnet_event_t *ev);
extern void reply_out_callback(lnet_event_t *ev);
extern void server_bulk_callback (lnet_event_t *ev);

/* ptlrpc/connection.c */
struct ptlrpc_connection *ptlrpc_connection_get(lnet_process_id_t peer,
                                                lnet_nid_t self,
                                                struct obd_uuid *uuid);
int ptlrpc_connection_put(struct ptlrpc_connection *c);
struct ptlrpc_connection *ptlrpc_connection_addref(struct ptlrpc_connection *);
int ptlrpc_connection_init(void);
void ptlrpc_connection_fini(void);
extern lnet_pid_t ptl_get_pid(void);

/* ptlrpc/niobuf.c */
int ptlrpc_start_bulk_transfer(struct ptlrpc_bulk_desc *desc);
void ptlrpc_abort_bulk(struct ptlrpc_bulk_desc *desc);
int ptlrpc_register_bulk(struct ptlrpc_request *req);
void ptlrpc_unregister_bulk (struct ptlrpc_request *req);

static inline int ptlrpc_bulk_active (struct ptlrpc_bulk_desc *desc)
{
        int           rc;

        spin_lock(&desc->bd_lock);
        rc = desc->bd_network_rw;
        spin_unlock(&desc->bd_lock);
        return (rc);
}

#define PTLRPC_REPLY_MAYBE_DIFFICULT 0x01
#define PTLRPC_REPLY_EARLY           0x02
int ptlrpc_send_reply(struct ptlrpc_request *req, int flags);
int ptlrpc_reply(struct ptlrpc_request *req);
int ptlrpc_send_error(struct ptlrpc_request *req, int difficult);
int ptlrpc_error(struct ptlrpc_request *req);
void ptlrpc_resend_req(struct ptlrpc_request *request);
int ptlrpc_at_get_net_latency(struct ptlrpc_request *req);
int ptl_send_rpc(struct ptlrpc_request *request, int noreply);
int ptlrpc_register_rqbd (struct ptlrpc_request_buffer_desc *rqbd);

/* ptlrpc/client.c */
void ptlrpc_init_client(int req_portal, int rep_portal, char *name,
                        struct ptlrpc_client *);
void ptlrpc_cleanup_client(struct obd_import *imp);
struct ptlrpc_connection *ptlrpc_uuid_to_connection(struct obd_uuid *uuid);

static inline int
ptlrpc_client_recv_or_unlink (struct ptlrpc_request *req)
{
        int           rc;

        spin_lock(&req->rq_lock);
        rc = req->rq_receiving_reply || req->rq_must_unlink;
        spin_unlock(&req->rq_lock);
        return (rc);
}

static inline void
ptlrpc_wake_client_req (struct ptlrpc_request *req)
{
        if (req->rq_set == NULL)
                cfs_waitq_signal(&req->rq_reply_waitq);
        else
                cfs_waitq_signal(&req->rq_set->set_waitq);
}

int ptlrpc_queue_wait(struct ptlrpc_request *req);
int ptlrpc_replay_req(struct ptlrpc_request *req);
void ptlrpc_unregister_reply(struct ptlrpc_request *req);
void ptlrpc_restart_req(struct ptlrpc_request *req);
void ptlrpc_abort_inflight(struct obd_import *imp);
void ptlrpc_abort_set(struct ptlrpc_request_set *set);

struct ptlrpc_request_set *ptlrpc_prep_set(void);
int ptlrpc_set_add_cb(struct ptlrpc_request_set *set,
                      set_interpreter_func fn, void *data);
int ptlrpc_set_next_timeout(struct ptlrpc_request_set *);
int ptlrpc_check_set(struct ptlrpc_request_set *set);
int ptlrpc_set_wait(struct ptlrpc_request_set *);
int ptlrpc_expired_set(void *data);
void ptlrpc_interrupted_set(void *data);
void ptlrpc_mark_interrupted(struct ptlrpc_request *req);
void ptlrpc_set_destroy(struct ptlrpc_request_set *);
void ptlrpc_set_add_req(struct ptlrpc_request_set *, struct ptlrpc_request *);
int ptlrpc_set_add_new_req(struct ptlrpcd_ctl *pc,
                           struct ptlrpc_request *req);

void ptlrpc_free_rq_pool(struct ptlrpc_request_pool *pool);
void ptlrpc_add_rqs_to_pool(struct ptlrpc_request_pool *pool, int num_rq);

struct ptlrpc_request_pool *
ptlrpc_init_rq_pool(int, int,
                    void (*populate_pool)(struct ptlrpc_request_pool *, int));

void ptlrpc_at_set_req_timeout(struct ptlrpc_request *req);
struct ptlrpc_request *ptlrpc_request_alloc(struct obd_import *imp,
                                            const struct req_format *format);
struct ptlrpc_request *ptlrpc_request_alloc_pool(struct obd_import *imp,
                                            struct ptlrpc_request_pool *,
                                            const struct req_format *format);
void ptlrpc_request_free(struct ptlrpc_request *request);
int ptlrpc_request_pack(struct ptlrpc_request *request,
                        __u32 version, int opcode);
struct ptlrpc_request *ptlrpc_request_alloc_pack(struct obd_import *imp,
                                                const struct req_format *format,
                                                __u32 version, int opcode);
int ptlrpc_request_bufs_pack(struct ptlrpc_request *request,
                             __u32 version, int opcode, char **bufs,
                             struct ptlrpc_cli_ctx *ctx);
struct ptlrpc_request *ptlrpc_prep_req(struct obd_import *imp, __u32 version,
                                       int opcode, int count, __u32 *lengths,
                                       char **bufs);
struct ptlrpc_request *ptlrpc_prep_req_pool(struct obd_import *imp,
                                             __u32 version, int opcode,
                                            int count, __u32 *lengths, char **bufs,
                                            struct ptlrpc_request_pool *pool);
void ptlrpc_req_finished(struct ptlrpc_request *request);
void ptlrpc_req_finished_with_imp_lock(struct ptlrpc_request *request);
struct ptlrpc_request *ptlrpc_request_addref(struct ptlrpc_request *req);
struct ptlrpc_bulk_desc *ptlrpc_prep_bulk_imp (struct ptlrpc_request *req,
                                               int npages, int type, int portal);
struct ptlrpc_bulk_desc *ptlrpc_prep_bulk_exp(struct ptlrpc_request *req,
                                              int npages, int type, int portal);
void ptlrpc_free_bulk(struct ptlrpc_bulk_desc *bulk);
void ptlrpc_prep_bulk_page(struct ptlrpc_bulk_desc *desc,
                           cfs_page_t *page, int pageoffset, int len);
void ptlrpc_retain_replayable_request(struct ptlrpc_request *req,
                                      struct obd_import *imp);
__u64 ptlrpc_next_xid(void);
__u64 ptlrpc_sample_next_xid(void);
__u64 ptlrpc_req_xid(struct ptlrpc_request *request);

struct ptlrpc_service_conf {
        int psc_nbufs;
        int psc_bufsize;
        int psc_max_req_size;
        int psc_max_reply_size;
        int psc_req_portal;
        int psc_rep_portal;
        int psc_watchdog_factor;
        int psc_min_threads;
        int psc_max_threads;
        __u32 psc_ctx_tags;
};

/* ptlrpc/service.c */
void ptlrpc_save_lock (struct ptlrpc_request *req,
                       struct lustre_handle *lock, int mode);
void ptlrpc_commit_replies (struct obd_device *obd);
void ptlrpc_schedule_difficult_reply (struct ptlrpc_reply_state *rs);
struct ptlrpc_service *ptlrpc_init_svc_conf(struct ptlrpc_service_conf *c,
                                            svc_handler_t h, char *name,
                                            struct proc_dir_entry *proc_entry,
                                            svcreq_printfn_t prntfn,
                                            char *threadname);

struct ptlrpc_service *ptlrpc_init_svc(int nbufs, int bufsize, int max_req_size,
                                       int max_reply_size,
                                       int req_portal, int rep_portal,
                                       int watchdog_factor,
                                       svc_handler_t, char *name,
                                       cfs_proc_dir_entry_t *proc_entry,
                                       svcreq_printfn_t,
                                       int min_threads, int max_threads,
                                       char *threadname, __u32 ctx_tags);
void ptlrpc_stop_all_threads(struct ptlrpc_service *svc);

int ptlrpc_start_threads(struct obd_device *dev, struct ptlrpc_service *svc);
int ptlrpc_start_thread(struct obd_device *dev, struct ptlrpc_service *svc);
int ptlrpc_unregister_service(struct ptlrpc_service *service);
int liblustre_check_services (void *arg);
void ptlrpc_daemonize(char *name);
int ptlrpc_service_health_check(struct ptlrpc_service *);


struct ptlrpc_svc_data {
        char *name;
        struct ptlrpc_service *svc;
        struct ptlrpc_thread *thread;
        struct obd_device *dev;
};

/* ptlrpc/import.c */
int ptlrpc_connect_import(struct obd_import *imp, char * new_uuid);
int ptlrpc_init_import(struct obd_import *imp);
int ptlrpc_disconnect_import(struct obd_import *imp, int noclose);
int ptlrpc_import_recovery_state_machine(struct obd_import *imp);

/* ptlrpc/pack_generic.c */
int ptlrpc_reconnect_import(struct obd_import *imp);
int lustre_msg_swabbed(struct lustre_msg *msg);
int lustre_msg_check_version(struct lustre_msg *msg, __u32 version);
void lustre_init_msg_v2(struct lustre_msg_v2 *msg, int count, __u32 *lens,
                        char **bufs);
int lustre_pack_request(struct ptlrpc_request *, __u32 magic, int count,
                        __u32 *lens, char **bufs);
int lustre_pack_reply(struct ptlrpc_request *, int count, __u32 *lens,
                      char **bufs);
int lustre_pack_reply_v2(struct ptlrpc_request *req, int count,
                         __u32 *lens, char **bufs, int flags);
#define LPRFL_EARLY_REPLY 1
int lustre_pack_reply_flags(struct ptlrpc_request *, int count, __u32 *lens,
                            char **bufs, int flags);
int lustre_shrink_msg(struct lustre_msg *msg, int segment,
                      unsigned int newlen, int move_data);
void lustre_free_reply_state(struct ptlrpc_reply_state *rs);
int lustre_msg_hdr_size(__u32 magic, int count);
int lustre_msg_size(__u32 magic, int count, __u32 *lengths);
int lustre_msg_size_v2(int count, __u32 *lengths);
int lustre_packed_msg_size(struct lustre_msg *msg);
int lustre_msg_early_size(void);
int lustre_unpack_msg(struct lustre_msg *m, int len);
void *lustre_msg_buf_v2(struct lustre_msg_v2 *m, int n, int min_size);
void *lustre_msg_buf(struct lustre_msg *m, int n, int minlen);
int lustre_msg_buflen(struct lustre_msg *m, int n);
void lustre_msg_set_buflen(struct lustre_msg *m, int n, int len);
int lustre_msg_bufcount(struct lustre_msg *m);
char *lustre_msg_string (struct lustre_msg *m, int n, int max_len);
void *lustre_swab_buf(struct lustre_msg *, int n, int minlen, void *swabber);
void *lustre_swab_reqbuf(struct ptlrpc_request *req, int n, int minlen,
                         void *swabber);
void *lustre_swab_repbuf(struct ptlrpc_request *req, int n, int minlen,
                         void *swabber);
__u32 lustre_msghdr_get_flags(struct lustre_msg *msg);
void lustre_msghdr_set_flags(struct lustre_msg *msg, __u32 flags);
__u32 lustre_msg_get_flags(struct lustre_msg *msg);
void lustre_msg_add_flags(struct lustre_msg *msg, int flags);
void lustre_msg_set_flags(struct lustre_msg *msg, int flags);
void lustre_msg_clear_flags(struct lustre_msg *msg, int flags);
__u32 lustre_msg_get_op_flags(struct lustre_msg *msg);
void lustre_msg_add_op_flags(struct lustre_msg *msg, int flags);
void lustre_msg_set_op_flags(struct lustre_msg *msg, int flags);
struct lustre_handle *lustre_msg_get_handle(struct lustre_msg *msg);
__u32 lustre_msg_get_type(struct lustre_msg *msg);
__u32 lustre_msg_get_version(struct lustre_msg *msg);
void lustre_msg_add_version(struct lustre_msg *msg, int version);
__u32 lustre_msg_get_opc(struct lustre_msg *msg);
__u64 lustre_msg_get_last_xid(struct lustre_msg *msg);
__u64 lustre_msg_get_last_committed(struct lustre_msg *msg);
__u64 lustre_msg_get_transno(struct lustre_msg *msg);
__u64 lustre_msg_get_slv(struct lustre_msg *msg);
__u32 lustre_msg_get_limit(struct lustre_msg *msg);
void lustre_msg_set_slv(struct lustre_msg *msg, __u64 slv);
void lustre_msg_set_limit(struct lustre_msg *msg, __u64 limit);
int lustre_msg_get_status(struct lustre_msg *msg);
__u32 lustre_msg_get_conn_cnt(struct lustre_msg *msg);
int lustre_msg_is_v1(struct lustre_msg *msg);
__u32 lustre_msg_get_magic(struct lustre_msg *msg);
__u32 lustre_msg_get_timeout(struct lustre_msg *msg);
__u32 lustre_msg_get_service_time(struct lustre_msg *msg);
__u32 lustre_msg_get_cksum(struct lustre_msg *msg);
__u32 lustre_msg_calc_cksum(struct lustre_msg *msg);
void lustre_msg_set_handle(struct lustre_msg *msg,struct lustre_handle *handle);
void lustre_msg_set_type(struct lustre_msg *msg, __u32 type);
void lustre_msg_set_opc(struct lustre_msg *msg, __u32 opc);
void lustre_msg_set_last_xid(struct lustre_msg *msg, __u64 last_xid);
void lustre_msg_set_last_committed(struct lustre_msg *msg,__u64 last_committed);
void lustre_msg_set_transno(struct lustre_msg *msg, __u64 transno);
void lustre_msg_set_status(struct lustre_msg *msg, __u32 status);
void lustre_msg_set_conn_cnt(struct lustre_msg *msg, __u32 conn_cnt);
void ptlrpc_req_set_repsize(struct ptlrpc_request *req, int count, __u32 *sizes);
void ptlrpc_request_set_replen(struct ptlrpc_request *req);
void lustre_msg_set_timeout(struct lustre_msg *msg, __u32 timeout);
void lustre_msg_set_service_time(struct lustre_msg *msg, __u32 service_time);
void lustre_msg_set_cksum(struct lustre_msg *msg, __u32 cksum);

static inline void
lustre_shrink_reply(struct ptlrpc_request *req, int segment,
                    unsigned int newlen, int move_data)
{
        LASSERT(req->rq_reply_state);
        LASSERT(req->rq_repmsg);
        req->rq_replen = lustre_shrink_msg(req->rq_repmsg, segment,
                                           newlen, move_data);
}

static inline void
ptlrpc_rs_addref(struct ptlrpc_reply_state *rs)
{
        LASSERT(atomic_read(&rs->rs_refcount) > 0);
        atomic_inc(&rs->rs_refcount);
}

static inline void
ptlrpc_rs_decref(struct ptlrpc_reply_state *rs)
{
        LASSERT(atomic_read(&rs->rs_refcount) > 0);
        if (atomic_dec_and_test(&rs->rs_refcount))
                lustre_free_reply_state(rs);
}

/* Should only be called once per req */
static inline void ptlrpc_req_drop_rs(struct ptlrpc_request *req)
{
        if (req->rq_reply_state == NULL)
                return; /* shouldn't occur */
        ptlrpc_rs_decref(req->rq_reply_state);
        req->rq_reply_state = NULL;
        req->rq_repmsg = NULL;
}

static inline __u32 lustre_request_magic(struct ptlrpc_request *req)
{
        return lustre_msg_get_magic(req->rq_reqmsg);
}

static inline int ptlrpc_req_get_repsize(struct ptlrpc_request *req)
{
        switch (req->rq_reqmsg->lm_magic) {
        case LUSTRE_MSG_MAGIC_V2:
                return req->rq_reqmsg->lm_repsize;
        default:
                LASSERTF(0, "incorrect message magic: %08x\n",
                         req->rq_reqmsg->lm_magic);
                return -EFAULT;
        }
}

/* ldlm/ldlm_lib.c */
int client_obd_setup(struct obd_device *obddev, struct lustre_cfg *lcfg);
int client_obd_cleanup(struct obd_device *obddev);
int client_connect_import(const struct lu_env *env,
                          struct lustre_handle *conn, struct obd_device *obd,
                          struct obd_uuid *cluuid, struct obd_connect_data *,
                          void *localdata);
int client_disconnect_export(struct obd_export *exp);
int client_import_add_conn(struct obd_import *imp, struct obd_uuid *uuid,
                           int priority);
int client_import_del_conn(struct obd_import *imp, struct obd_uuid *uuid);
int import_set_conn_priority(struct obd_import *imp, struct obd_uuid *uuid);

/* ptlrpc/pinger.c */
int ptlrpc_pinger_add_import(struct obd_import *imp);
int ptlrpc_pinger_del_import(struct obd_import *imp);
cfs_time_t ptlrpc_suspend_wakeup_time(void);
#ifdef __KERNEL__
void ping_evictor_start(void);
void ping_evictor_stop(void);
#else
#define ping_evictor_start()    do {} while (0)
#define ping_evictor_stop()     do {} while (0)
#endif
int ptlrpc_check_and_wait_suspend(struct ptlrpc_request *req);

/* ptlrpc/ptlrpcd.c */
int ptlrpcd_start(char *name, struct ptlrpcd_ctl *pc);
void ptlrpcd_stop(struct ptlrpcd_ctl *pc, int force);
void ptlrpcd_wake(struct ptlrpc_request *req);
void ptlrpcd_add_req(struct ptlrpc_request *req);
int ptlrpcd_addref(void);
void ptlrpcd_decref(void);

/* ptlrpc/lproc_ptlrpc.c */
const char* ll_opcode2str(__u32 opcode);
#ifdef LPROCFS
void ptlrpc_lprocfs_register_obd(struct obd_device *obd);
void ptlrpc_lprocfs_unregister_obd(struct obd_device *obd);
void ptlrpc_lprocfs_brw(struct ptlrpc_request *req, int bytes);
#else
static inline void ptlrpc_lprocfs_register_obd(struct obd_device *obd) {}
static inline void ptlrpc_lprocfs_unregister_obd(struct obd_device *obd) {}
static inline void ptlrpc_lprocfs_brw(struct ptlrpc_request *req, int bytes) {}
#endif

/* ptlrpc/llog_server.c */
int llog_origin_handle_create(struct ptlrpc_request *req);
int llog_origin_handle_destroy(struct ptlrpc_request *req);
int llog_origin_handle_prev_block(struct ptlrpc_request *req);
int llog_origin_handle_next_block(struct ptlrpc_request *req);
int llog_origin_handle_read_header(struct ptlrpc_request *req);
int llog_origin_handle_close(struct ptlrpc_request *req);
int llog_origin_handle_cancel(struct ptlrpc_request *req);
int llog_catinfo(struct ptlrpc_request *req);

/* ptlrpc/llog_client.c */
extern struct llog_operations llog_client_ops;

#endif
