/* keylist.c -  key listing
 *	Copyright (C) 2000 Werner Koch (dd9jn)
 *      Copyright (C) 2001, 2002 g10 Code GmbH
 *
 * This file is part of GPGME.
 *
 * GPGME is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GPGME is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "util.h"
#include "context.h"
#include "ops.h"
#include "key.h"

#define my_isdigit(a) ( (a) >='0' && (a) <= '9' )

struct keylist_result_s
{
  int truncated;
  GpgmeData xmlinfo;
};

static void finish_key ( GpgmeCtx ctx );


void
_gpgme_release_keylist_result (KeylistResult result)
{
  if (!result)
    return;
  xfree (result);
}

/* Append some XML info.  args is currently ignore but we might want
   to add more information in the future (like source of the
   keylisting.  With args of NULL the XML structure is closed.  */
static void
append_xml_keylistinfo (GpgmeData *rdh, char *args)
{
  GpgmeData dh;

  if (!*rdh)
    {
      if (gpgme_data_new (rdh))
	return; /* FIXME: We are ignoring out-of-core.  */
      dh = *rdh;
      _gpgme_data_append_string (dh, "<GnupgOperationInfo>\n");
    }
  else
    {
      dh = *rdh;
      _gpgme_data_append_string (dh, "  </keylisting>\n");
    }

  if (!args)
    {
      /* Just close the XML containter.  */
      _gpgme_data_append_string (dh, "</GnupgOperationInfo>\n");
      return;
    }

  _gpgme_data_append_string (dh,
                             "  <keylisting>\n"
                             "    <truncated/>\n"
			     );
    
}



static void
keylist_status_handler (GpgmeCtx ctx, GpgmeStatusCode code, char *args)
{
  if (ctx->error)
    return;
  test_and_allocate_result (ctx, keylist);

   switch (code)
    {
    case GPGME_STATUS_TRUNCATED:
      ctx->result.keylist->truncated = 1;
      break;

    case GPGME_STATUS_EOF:
      if (ctx->result.keylist->truncated)
        append_xml_keylistinfo (&ctx->result.keylist->xmlinfo, "1");
      if (ctx->result.keylist->xmlinfo)
	{
	  append_xml_keylistinfo (&ctx->result.keylist->xmlinfo, NULL);
	  _gpgme_set_op_info (ctx, ctx->result.keylist->xmlinfo);
	  ctx->result.keylist->xmlinfo = NULL;
        }
      break;

    default:
      /* Ignore all other codes.  */
      break;
    }
}


static time_t
parse_timestamp (char *p)
{
  if (!*p)
    return 0;

  return (time_t)strtoul (p, NULL, 10);
}


static void
set_mainkey_trust_info (GpgmeKey key, const char *s)
{
  /* Look at letters and stop at the first digit.  */
  for (; *s && !my_isdigit (*s); s++)
    {
      switch (*s)
	{
	case 'e': key->keys.flags.expired = 1; break;
	case 'r': key->keys.flags.revoked = 1; break;
	case 'd': key->keys.flags.disabled = 1; break;
	case 'i': key->keys.flags.invalid = 1; break;
        }
    }
}


static void
set_userid_flags (GpgmeKey key, const char *s)
{
  struct user_id_s *u = key->uids;

  assert (u);
  while (u->next)
    u = u->next;

  /* Look at letters and stop at the first digit.  */
  for (; *s && !my_isdigit (*s); s++)
    {
      switch (*s)
	{
	case 'r': u->revoked  = 1; break;
	case 'i': u->invalid  = 1; break;

	case 'n': u->validity = GPGME_VALIDITY_NEVER; break;
	case 'm': u->validity = GPGME_VALIDITY_MARGINAL; break;
	case 'f': u->validity = GPGME_VALIDITY_FULL; break;
	case 'u': u->validity = GPGME_VALIDITY_ULTIMATE; break;
        }
    }
}


static void
set_subkey_trust_info (struct subkey_s *k, const char *s)
{
  /* Look at letters and stop at the first digit.  */
  for (; *s && !my_isdigit (*s); s++)
    {
      switch (*s)
	{
	case 'e': k->flags.expired = 1; break;
	case 'r': k->flags.revoked = 1; break;
	case 'd': k->flags.disabled = 1; break;
	case 'i': k->flags.invalid = 1; break;
        }
    }
}


static void
set_mainkey_capability (GpgmeKey key, const char *s)
{
  for (; *s ; s++)
    {
      switch (*s)
	{
	case 'e': key->keys.flags.can_encrypt = 1; break;
	case 's': key->keys.flags.can_sign = 1; break;
	case 'c': key->keys.flags.can_certify = 1; break;
	case 'E': key->gloflags.can_encrypt = 1; break;
	case 'S': key->gloflags.can_sign = 1; break;
	case 'C': key->gloflags.can_certify = 1; break;
        }
    }
}


static void
set_subkey_capability ( struct subkey_s *k, const char *s)
{
  for (; *s; s++)
    {
      switch (*s)
	{
	case 'e': k->flags.can_encrypt = 1; break;
	case 's': k->flags.can_sign = 1; break;
	case 'c': k->flags.can_certify = 1; break;
        }
    }
}

static void
set_ownertrust (GpgmeKey key, const char *s)
{
  /* Look at letters and stop at the first digit.  */
  for (; *s && !my_isdigit (*s); s++)
    {
      switch (*s)
	{
	case 'n': key->otrust = GPGME_VALIDITY_NEVER; break;
	case 'm': key->otrust = GPGME_VALIDITY_MARGINAL; break;
	case 'f': key->otrust = GPGME_VALIDITY_FULL; break;
	case 'u': key->otrust = GPGME_VALIDITY_ULTIMATE; break;
        default : key->otrust = GPGME_VALIDITY_UNKNOWN; break;
        }
    }
}


/* Note: We are allowed to modify LINE.  */
static void
keylist_colon_handler (GpgmeCtx ctx, char *line)
{
  char *p, *pend;
  int field = 0;
  enum
    {
      RT_NONE, RT_SIG, RT_UID, RT_SUB, RT_PUB, RT_FPR, RT_SSB, RT_SEC,
      RT_CRT, RT_CRS
    }
  rectype = RT_NONE;
  GpgmeKey key = ctx->tmp_key;
  int i;
  const char *trust_info = NULL;
  struct subkey_s *sk = NULL;

  DEBUG3 ("keylist_colon_handler ctx=%p, key=%p, line=%s\n", ctx, key,
          line? line: "(null)");
  if (ctx->error)
    return;
  if (!line)
    {
      /* EOF */
      finish_key (ctx);
      return; 
    }
  
  for (p = line; p; p = pend)
    {
      field++;
      pend = strchr (p, ':');
      if (pend) 
	*pend++ = 0;

      if (field == 1)
	{
	  if (!strcmp (p, "sig"))
	    rectype = RT_SIG;
	  else if (!strcmp (p, "uid") && key)
	    {
	      rectype = RT_UID;
	      key = ctx->tmp_key;
            }
	  else if (!strcmp (p, "sub") && key)
	    {
	      /* Start a new subkey.  */
	      rectype = RT_SUB; 
	      if (!(sk = _gpgme_key_add_subkey (key)))
		{
		  ctx->error = mk_error (Out_Of_Core);
		  return;
                }
            }
	  else if (!strcmp (p, "ssb") && key)
	    {
	      /* Start a new secret subkey.  */
	      rectype = RT_SSB;
	      if (!(sk = _gpgme_key_add_secret_subkey (key)))
		{
		  ctx->error = mk_error (Out_Of_Core);
		  return;
                }
            }
	  else if (!strcmp (p, "pub"))
	    {
	      /* Start a new keyblock.  */
	      if (_gpgme_key_new (&key))
		{
		  /* The only kind of error we can get.  */
		  ctx->error = mk_error (Out_Of_Core);
		  return;
                }
	      rectype = RT_PUB;
	      finish_key (ctx);
	      assert (!ctx->tmp_key);
	      ctx->tmp_key = key;
            }
	  else if (!strcmp (p, "sec"))
	    {
	      /* Start a new keyblock,  */
	      if (_gpgme_key_new_secret (&key))
		{
		  /* The only kind of error we can get.  */
		  ctx->error = mk_error (Out_Of_Core);
		  return;
		}
	      rectype = RT_SEC;
	      finish_key (ctx);
	      assert (!ctx->tmp_key);
	      ctx->tmp_key = key;
            }
	  else if (!strcmp (p, "crt"))
	    {
	      /* Start a new certificate. */
	      if (_gpgme_key_new (&key))
		{
		  /* The only kind of error we can get.  */
		  ctx->error = mk_error (Out_Of_Core);
		  return;
		}
	      key->x509 = 1;
	      rectype = RT_CRT;
	      finish_key (ctx);
	      assert (!ctx->tmp_key);
	      ctx->tmp_key = key;
            }
	  else if (!strcmp (p, "crs"))
	    {
	      /* Start a new certificate.  */
	      if (_gpgme_key_new_secret (&key))
		{
		  /* The only kind of error we can get.  */
		  ctx->error = mk_error (Out_Of_Core);
		  return;
                }
	      key->x509 = 1;
	      rectype = RT_CRS;
	      finish_key (ctx);
	      assert (!ctx->tmp_key);
	      ctx->tmp_key = key;
            }
	  else if (!strcmp (p, "fpr") && key) 
	    rectype = RT_FPR;
	  else 
	    rectype = RT_NONE;
        }
      else if (rectype == RT_PUB || rectype == RT_SEC
	       || rectype == RT_CRT || rectype == RT_CRS)
	{
	  switch (field)
	    {
	    case 2: /* trust info */
	      trust_info = p; 
	      set_mainkey_trust_info (key, trust_info);
	      break;
	    case 3: /* key length */
	      i = atoi (p); 
	      if (i > 1) /* ignore invalid values */
		key->keys.key_len = i; 
                break;
	    case 4: /* pubkey algo */
	      i = atoi (p);
	      if (i >= 1 && i < 128)
		key->keys.key_algo = i;
	      break;
              case 5: /* long keyid */
                if (strlen (p) == DIM(key->keys.keyid) - 1)
		  strcpy (key->keys.keyid, p);
                break;
	    case 6: /* timestamp (seconds) */
	      key->keys.timestamp = parse_timestamp (p);
	      break;
	    case 7: /* expiration time (seconds) */
	      key->keys.expires_at = parse_timestamp (p);
	      break;
	    case 8: /* X.509 serial number */
              if (rectype == RT_CRT || rectype == RT_CRS)
                {
                  key->issuer_serial = xtrystrdup (p);
		  if (!key->issuer_serial)
		    ctx->error = mk_error (Out_Of_Core);
                }
	      break;
	    case 9: /* ownertrust */
              set_ownertrust (key, p);
	      break;
	    case 10:
	      /* Not used for gpg due to --fixed-list-mode option but
		 GPGSM stores the issuer name.  */
              if (rectype == RT_CRT || rectype == RT_CRS)
		if (_gpgme_decode_c_string (p, &key->issuer_name))
		  ctx->error = mk_error (Out_Of_Core);
	      break;
	    case 11: /* signature class  */
	      break;
	    case 12: /* capabilities */
	      set_mainkey_capability (key, p);
	      break;
	    case 13:
	      pend = NULL;  /* we can stop here */
	      break;
            }
          }
        else if ((rectype == RT_SUB || rectype== RT_SSB) && sk)
	  {
            switch (field)
	      {
              case 2: /* trust info */
                set_subkey_trust_info (sk, p);
                break;
              case 3: /* key length */
                i = atoi (p); 
                if (i > 1) /* ignore invalid values */
		  sk->key_len = i; 
                break;
              case 4: /* pubkey algo */
                i = atoi (p);
                if (i >= 1 && i < 128)
		  sk->key_algo = i;
                break;
              case 5: /* long keyid */
                if (strlen (p) == DIM(sk->keyid) - 1)
		  strcpy (sk->keyid, p);
                break;
              case 6: /* timestamp (seconds) */
                sk->timestamp = parse_timestamp (p);
                break;
              case 7: /* expiration time (seconds) */
                sk->expires_at = parse_timestamp (p);
                break;
              case 8: /* reserved (LID) */
                break;
              case 9: /* ownertrust */
                break;
              case 10:/* user ID n/a for a subkey */
                break;
              case 11:  /* signature class  */
                break;
              case 12: /* capability */
                set_subkey_capability (sk, p);
                break;
              case 13:
                pend = NULL;  /* we can stop here */
                break;
            }
        }
      else if (rectype == RT_UID)
	{
	  switch (field)
	    {
	    case 2: /* trust info */
	      trust_info = p;  /*save for later */
	      break;
	    case 10: /* user ID */
	      if (_gpgme_key_append_name (key, p))
		/* The only kind of error we can get*/
		ctx->error = mk_error (Out_Of_Core);
	      else
		{
		  if (trust_info)
                    set_userid_flags (key, trust_info);
		}
	      pend = NULL;  /* we can stop here */
	      break;
            }
        }
      else if (rectype == RT_FPR)
	{
	  switch (field)
	    {
	    case 10: /* fingerprint (take only the first one)*/
	      if (!key->keys.fingerprint && *p)
		{
		  key->keys.fingerprint = xtrystrdup (p);
		  if (!key->keys.fingerprint)
		    ctx->error = mk_error (Out_Of_Core);
                }
	      break;
	    case 13: /* gpgsm chain ID (take only the first one)*/
	      if (!key->chain_id && *p)
		{
		  key->chain_id = xtrystrdup (p);
		  if (!key->chain_id)
		    ctx->error = mk_error (Out_Of_Core);
                }
	      pend = NULL; /* that is all we want */
	      break;
            }
        }
    }
}


/*
 * We have read an entire key into ctx->tmp_key and should now finish
 * it.  It is assumed that this releases ctx->tmp_key.
 */
static void
finish_key (GpgmeCtx ctx)
{
  GpgmeKey key = ctx->tmp_key;

  ctx->tmp_key = NULL;

  if (key)
    _gpgme_engine_io_event (ctx->engine, GPGME_EVENT_NEXT_KEY, key);
}


void
_gpgme_op_keylist_event_cb (void *data, GpgmeEventIO type, void *type_data)
{
  GpgmeCtx ctx = (GpgmeCtx) data;
  GpgmeKey key = (GpgmeKey) type_data;
  struct key_queue_item_s *q, *q2;

  assert (type == GPGME_EVENT_NEXT_KEY);

  _gpgme_key_cache_add (key);

  q = xtrymalloc (sizeof *q);
  if (!q)
    {
      gpgme_key_release (key);
      ctx->error = mk_error (Out_Of_Core);
      return;
    }
  q->key = key;
  q->next = NULL;
  /* FIXME: Lock queue.  Use a tail pointer?  */
  if (!(q2 = ctx->key_queue))
    ctx->key_queue = q;
  else
    {
      for (; q2->next; q2 = q2->next)
	;
      q2->next = q;
    }
  ctx->key_cond = 1;
  /* FIXME: Unlock queue.  */
}


/**
 * gpgme_op_keylist_start:
 * @c: context 
 * @pattern: a GnuPG user ID or NULL for all
 * @secret_only: List only keys where the secret part is available
 * 
 * Note that this function also cancels a pending key listing
 * operaton. To actually retrieve the key, use
 * gpgme_op_keylist_next().
 * 
 * Return value:  0 on success or an errorcode. 
 **/
GpgmeError
gpgme_op_keylist_start (GpgmeCtx ctx, const char *pattern, int secret_only)
{
  GpgmeError err = 0;

  err = _gpgme_op_reset (ctx, 2);
  if (err)
    goto leave;

  gpgme_key_release (ctx->tmp_key);
  ctx->tmp_key = NULL;
  /* Fixme: Release key_queue.  */

  _gpgme_engine_set_status_handler (ctx->engine, keylist_status_handler, ctx);
  err = _gpgme_engine_set_colon_line_handler (ctx->engine,
					      keylist_colon_handler, ctx);
  if (err)
    goto leave;

  /* We don't want to use the verbose mode as this will also print 
     the key signatures which is in most cases not needed and furthermore we 
     just ignore those lines - This should speed up things */
  _gpgme_engine_set_verbosity (ctx->engine, 0);

  err = _gpgme_engine_op_keylist (ctx->engine, pattern, secret_only,
				  ctx->keylist_mode);

  if (!err)	/* And kick off the process.  */
    err = _gpgme_engine_start (ctx->engine, ctx);

 leave:
  if (err)
    {
      ctx->pending = 0; 
      _gpgme_engine_release (ctx->engine);
      ctx->engine = NULL;
    }
  return err;
}


/**
 * gpgme_op_keylist_ext_start:
 * @c: context 
 * @pattern: a NULL terminated array of search patterns
 * @secret_only: List only keys where the secret part is available
 * @reserved: Should be 0.
 * 
 * Note that this function also cancels a pending key listing
 * operaton. To actually retrieve the key, use
 * gpgme_op_keylist_next().
 * 
 * Return value:  0 on success or an errorcode. 
 **/
GpgmeError
gpgme_op_keylist_ext_start (GpgmeCtx ctx, const char *pattern[],
			    int secret_only, int reserved)
{
  GpgmeError err = 0;

  err = _gpgme_op_reset (ctx, 2);
  if (err)
    goto leave;

  gpgme_key_release (ctx->tmp_key);
  ctx->tmp_key = NULL;

  _gpgme_engine_set_status_handler (ctx->engine, keylist_status_handler, ctx);
  err = _gpgme_engine_set_colon_line_handler (ctx->engine,
					      keylist_colon_handler, ctx);
  if (err)
    goto leave;

  /* We don't want to use the verbose mode as this will also print 
     the key signatures which is in most cases not needed and furthermore we 
     just ignore those lines - This should speed up things */
  _gpgme_engine_set_verbosity (ctx->engine, 0);

  err = _gpgme_engine_op_keylist_ext (ctx->engine, pattern, secret_only,
				      reserved, ctx->keylist_mode);

  if (!err)	/* And kick off the process.  */
    err = _gpgme_engine_start (ctx->engine, ctx);

 leave:
  if (err)
    {
      ctx->pending = 0; 
      _gpgme_engine_release (ctx->engine);
      ctx->engine = NULL;
    }
  return err;
}


/**
 * gpgme_op_keylist_next:
 * @c: Context
 * @r_key: Returned key object
 * 
 * Return the next key from the key listing started with
 * gpgme_op_keylist_start().  The caller must free the key using
 * gpgme_key_release().  If the last key has already been returned the
 * last time the function was called, %GPGME_EOF is returned and the
 * operation is finished.
 * 
 * Return value: 0 on success, %GPGME_EOF or another error code.
 **/
GpgmeError
gpgme_op_keylist_next (GpgmeCtx ctx, GpgmeKey *r_key)
{
  struct key_queue_item_s *queue_item;

  if (!r_key)
    return mk_error (Invalid_Value);
  *r_key = NULL;
  if (!ctx)
    return mk_error (Invalid_Value);
  if (!ctx->pending)
    return mk_error (No_Request);
  if (ctx->error)
    return ctx->error;

  if (!ctx->key_queue)
    {
      GpgmeError err = _gpgme_wait_on_condition (ctx, &ctx->key_cond);
      if (err)
	{
	  ctx->pending = 0;
	  return err;
	}
      if (!ctx->pending)
	{
	  /* The operation finished.  Because not all keys might have
	     been returned to the caller yet, we just reset the
	     pending flag to 1.  This will cause us to call
	     _gpgme_wait_on_condition without any active file
	     descriptors, but that is a no-op, so it is safe.  */
	  ctx->pending = 1;
	}
      if (!ctx->key_cond)
	{
	  ctx->pending = 0;
	  return mk_error (EOF);
	}
      ctx->key_cond = 0; 
      assert (ctx->key_queue);
    }
  queue_item = ctx->key_queue;
  ctx->key_queue = queue_item->next;
  if (!ctx->key_queue)
    ctx->key_cond = 0;
  
  *r_key = queue_item->key;
  xfree (queue_item);
  return 0;
}


/**
 * gpgme_op_keylist_end:
 * @c: Context
 * 
 * Ends the keylist operation and allows to use the context for some
 * other operation next.
 **/
GpgmeError
gpgme_op_keylist_end (GpgmeCtx ctx)
{
  if (!ctx)
    return mk_error (Invalid_Value);
  if (!ctx->pending)
    return mk_error (No_Request);
  if (ctx->error)
    return ctx->error;

  ctx->pending = 0;
  return 0;
}
