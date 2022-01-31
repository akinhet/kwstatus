#include "kwstatus.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>

/* global variables */
pthread_cond_t cupdate;
pthread_mutex_t mupdate;

static void
die(char* str) {
  fputs(str, stderr);
  exit(1);
}

void
warn(char* warning) {
  fputs(warning, stderr);
}
void*
emalloc(size_t size) {
  void* ret;
  ret = malloc(size);
  if(ret == NULL)
    die("malloc failed");
  return ret;
}

void*
ecalloc(size_t nmemb, size_t size) {
  void* ret;
  ret = calloc(nmemb, size);
  if(ret == NULL)
    die("calloc failed");
  return ret;
}

void mod_update(struct modules* self, const char* out) {
  pthread_mutex_lock(&self->mut);
  strncpy(self->out, out, MODSIZE);
  if(!self->no_delim && self->out[0])
    strncat(self->out, delim, MODSIZE);
  pthread_mutex_unlock(&self->mut);

  pthread_mutex_lock(&mupdate);
  pthread_cond_signal(&cupdate);
  pthread_mutex_unlock(&mupdate);
}

static void
init_modules(pthread_t* thr, size_t len) {
  size_t n;

  for(n=0; n<len; n++) {
    pthread_mutex_init(&mdl[n].mut, NULL);
    mdl[n].out = ecalloc(MODSIZE, 1);
    pthread_create(&thr[n], NULL, (void*)mdl[n].fun, &mdl[n]);
  }
}
int
main(int argc, char* argv[]) {
  size_t mlen = LENGTH(mdl);
  size_t n;
  short dry = 0;
  char out[BARSIZE] = {0};
  pthread_t thr[mlen];
  xcb_connection_t *c;
  xcb_window_t root;
  const struct timespec update_delay = {0, align_ms*1000000};

  /* check args */
  if(argc >= 2 && !strcmp(argv[1], "-d"))
      dry = 1;

  /* initialization */
  pthread_cond_init(&cupdate, NULL);
  pthread_mutex_init(&mupdate, NULL);
  init_modules(thr, mlen);
  if(!dry) {
    c = xcb_connect(NULL, NULL);
    if(!c)
      die("Cannot connect to X server");
    root = xcb_setup_roots_iterator(xcb_get_setup(c)).data->root;
  }

  pthread_mutex_lock(&mupdate);
  /* main loop */
  while(1) {
    /* wait until next event */
    pthread_cond_wait(&cupdate, &mupdate);

    /* wait to align parallel modules with similar timings to avoid unnecessary updates */
    pthread_mutex_unlock(&mupdate);
    nanosleep(&update_delay, NULL);
    pthread_mutex_lock(&mupdate);

    /* read all modules */
    out[0] = 0;
    for(n = 0; n < mlen; n++) {
      pthread_mutex_lock(&mdl[n].mut);
        strncat(out, mdl[n].out, BARSIZE);
      pthread_mutex_unlock(&mdl[n].mut);
    }

    /* update bar */
    if(dry)
      puts(out);
    else {
      xcb_change_property(c, XCB_PROP_MODE_REPLACE, root, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(out), out);
      xcb_flush(c);
    }
  }

	return 0;
}

