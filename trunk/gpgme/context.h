/* context.h
   Copyright (C) 2000 Werner Koch (dd9jn)
   Copyright (C) 2001, 2002, 2003 g10 Code GmbH

   This file is part of GPGME.
 
   GPGME is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 
   GPGME is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
 
   You should have received a copy of the GNU General Public License
   along with GPGME; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef CONTEXT_H
#define CONTEXT_H

#include "gpgme.h"
#include "types.h"
#include "engine.h"
#include "wait.h"

struct key_queue_item_s
{
  struct key_queue_item_s *next;
  GpgmeKey key;
};

struct trust_queue_item_s
{
  struct trust_queue_item_s *next;
  GpgmeTrustItem item;
};


/* Currently we need it at several places, so we put the definition
   into this header file.  */
struct gpgme_context_s
{
  int initialized;
  /* An engine request is still pending.  */
  int pending;

  int use_cms;

  /* Cancel operation requested.  */
  int cancel;

  /* The running engine process.  */
  EngineObject engine;

  /* Level of verbosity to use.  */
  int verbosity;
  int use_armor;  
  int use_textmode;
  int keylist_mode;
  int include_certs;

  /* The number of keys in signers.  */
  int signers_len;
  /* Size of the following array.  */
  int signers_size;
  GpgmeKey *signers;

  struct
  {
    VerifyResult verify;
    DecryptResult decrypt;
    SignResult sign;
    EncryptResult encrypt;
    PassphraseResult passphrase;
    ImportResult import;
    DeleteResult delete;
    GenKeyResult genkey;
    KeylistResult keylist;
    EditResult edit;
  } result;

  /* Last signature notation.  */
  GpgmeData notation;
  /* Last operation info.  */
  GpgmeData op_info;

  /* Used by keylist.c.  */
  GpgmeKey tmp_key;
  struct user_id_s *tmp_uid;
  /* Something new is available.  */
  volatile int key_cond;
  struct key_queue_item_s *key_queue;
  struct trust_queue_item_s *trust_queue;

  GpgmePassphraseCb passphrase_cb;
  void *passphrase_cb_value;

  GpgmeProgressCb progress_cb;
  void *progress_cb_value;

  /* A list of file descriptors in active use by the current
     operation.  */
  struct fd_table fdt;
  struct GpgmeIOCbs io_cbs;
  
  GpgmeData help_data_1;
};

/* Forward declaration of a structure to store certification
   signatures.  */
struct certsig_s;

/* Structure to store user IDs.  */
struct user_id_s
{
  struct user_id_s *next;
  unsigned int revoked : 1;
  unsigned int invalid : 1;
  GpgmeValidity validity; 
  struct certsig_s *certsigs;
  struct certsig_s *last_certsig;
  const char *name_part;	/* All 3 point into strings behind name  */
  const char *email_part;	/* or to read-only strings.  */
  const char *comment_part;
  char name[1];
};


struct gpgme_recipients_s
{
  struct user_id_s *list;
  int checked;	/* Wether the recipients are all valid.  */
};


#define fail_on_pending_request(c)                            \
          do {                                                \
                if (!(c))         return GPGME_Invalid_Value; \
                if ((c)->pending) return GPGME_Busy;          \
             } while (0)

#endif	/* CONTEXT_H */
