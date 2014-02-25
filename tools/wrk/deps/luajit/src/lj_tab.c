/*
** Table handling.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#define lj_tab_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_tab.h"

/* -- Object hashing ------------------------------------------------------ */

/* Hash values are masked with the table hash mask and used as an index. */
static LJ_AINLINE Node *hashmask(const GCtab *t, uint32_t hash)
{
  Node *n = noderef(t->node);
  return &n[hash & t->hmask];
}

/* String hashes are precomputed when they are interned. */
#define hashstr(t, s)		hashmask(t, (s)->hash)

#define hashlohi(t, lo, hi)	hashmask((t), hashrot((lo), (hi)))
#define hashnum(t, o)		hashlohi((t), (o)->u32.lo, ((o)->u32.hi << 1))
#define hashptr(t, p)		hashlohi((t), u32ptr(p), u32ptr(p) + HASH_BIAS)
#define hashgcref(t, r)		hashlohi((t), gcrefu(r), gcrefu(r) + HASH_BIAS)

/* Hash an arbitrary key and return its anchor position in the hash table. */
static Node *hashkey(const GCtab *t, cTValue *key)
{
  lua_assert(!tvisint(key));
  if (tvisstr(key))
    return hashstr(t, strV(key));
  else if (tvisnum(key))
    return hashnum(t, key);
  else if (tvisbool(key))
    return hashmask(t, boolV(key));
  else
    return hashgcref(t, key->gcr);
  /* Only hash 32 bits of lightuserdata on a 64 bit CPU. Good enough? */
}

/* -- Table creation and destruction -------------------------------------- */

/* Create new hash part for table. */
static LJ_AINLINE void newhpart(lua_State *L, GCtab *t, uint32_t hbits)
{
  uint32_t hsize;
  Node *node;
  lua_assert(hbits != 0);
  if (hbits > LJ_MAX_HBITS)
    lj_err_msg(L, LJ_ERR_TABOV);
  hsize = 1u << hbits;
  node = lj_mem_newvec(L, hsize, Node);
  setmref(node->freetop, &node[hsize]);
  setmref(t->node, node);
  t->hmask = hsize-1;
}

/*
** Q: Why all of these copies of t->hmask, t->node etc. to local variables?
** A: Because alias analysis for C is _really_ tough.
**    Even state-of-the-art C compilers won't produce good code without this.
*/

/* Clear hash part of table. */
static LJ_AINLINE void clearhpart(GCtab *t)
{
  uint32_t i, hmask = t->hmask;
  Node *node = noderef(t->node);
  lua_assert(t->hmask != 0);
  for (i = 0; i <= hmask; i++) {
    Node *n = &node[i];
    setmref(n->next, NULL);
    setnilV(&n->key);
    setnilV(&n->val);
  }
}

/* Clear array part of table. */
static LJ_AINLINE void clearapart(GCtab *t)
{
  uint32_t i, asize = t->asize;
  TValue *array = tvref(t->array);
  for (i = 0; i < asize; i++)
    setnilV(&array[i]);
}

/* Create a new table. Note: the slots are not initialized (yet). */
static GCtab *newtab(lua_State *L, uint32_t asize, uint32_t hbits)
{
  GCtab *t;
  /* First try to colocate the array part. */
  if (LJ_MAX_COLOSIZE != 0 && asize > 0 && asize <= LJ_MAX_COLOSIZE) {
    lua_assert((sizeof(GCtab) & 7) == 0);
    t = (GCtab *)lj_mem_newgco(L, sizetabcolo(asize));
    t->gct = ~LJ_TTAB;
    t->nomm = (uint8_t)~0;
    t->colo = (int8_t)asize;
    setmref(t->array, (TValue *)((char *)t + sizeof(GCtab)));
    setgcrefnull(t->metatable);
    t->asize = asize;
    t->hmask = 0;
    setmref(t->node, &G(L)->nilnode);
  } else {  /* Otherwise separately allocate the array part. */
    t = lj_mem_newobj(L, GCtab);
    t->gct = ~LJ_TTAB;
    t->nomm = (uint8_t)~0;
    t->colo = 0;
    setmref(t->array, NULL);
    setgcrefnull(t->metatable);
    t->asize = 0;  /* In case the array allocation fails. */
    t->hmask = 0;
    setmref(t->node, &G(L)->nilnode);
    if (asize > 0) {
      if (asize > LJ_MAX_ASIZE)
	lj_err_msg(L, LJ_ERR_TABOV);
      setmref(t->array, lj_mem_newvec(L, asize, TValue));
      t->asize = asize;
    }
  }
  if (hbits)
    newhpart(L, t, hbits);
  return t;
}

/* Create a new table.
**
** IMPORTANT NOTE: The API differs from lua_createtable()!
**
** The array size is non-inclusive. E.g. asize=128 creates array slots
** for 0..127, but not for 128. If you need slots 1..128, pass asize=129
** (slot 0 is wasted in this case).
**
** The hash size is given in hash bits. hbits=0 means no hash part.
** hbits=1 creates 2 hash slots, hbits=2 creates 4 hash slots and so on.
*/
GCtab *lj_tab_new(lua_State *L, uint32_t asize, uint32_t hbits)
{
  GCtab *t = newtab(L, asize, hbits);
  clearapart(t);
  if (t->hmask > 0) clearhpart(t);
  return t;
}

#if LJ_HASJIT
GCtab * LJ_FASTCALL lj_tab_new1(lua_State *L, uint32_t ahsize)
{
  GCtab *t = newtab(L, ahsize & 0xffffff, ahsize >> 24);
  clearapart(t);
  if (t->hmask > 0) clearhpart(t);
  return t;
}
#endif

/* Duplicate a table. */
GCtab * LJ_FASTCALL lj_tab_dup(lua_State *L, const GCtab *kt)
{
  GCtab *t;
  uint32_t asize, hmask;
  t = newtab(L, kt->asize, kt->hmask > 0 ? lj_fls(kt->hmask)+1 : 0);
  lua_assert(kt->asize == t->asize && kt->hmask == t->hmask);
  t->nomm = 0;  /* Keys with metamethod names may be present. */
  asize = kt->asize;
  if (asize > 0) {
    TValue *array = tvref(t->array);
    TValue *karray = tvref(kt->array);
    if (asize < 64) {  /* An inlined loop beats memcpy for < 512 bytes. */
      uint32_t i;
      for (i = 0; i < asize; i++)
	copyTV(L, &array[i], &karray[i]);
    } else {
      memcpy(array, karray, asize*sizeof(TValue));
    }
  }
  hmask = kt->hmask;
  if (hmask > 0) {
    uint32_t i;
    Node *node = noderef(t->node);
    Node *knode = noderef(kt->node);
    ptrdiff_t d = (char *)node - (char *)knode;
    setmref(node->freetop, (Node *)((char *)noderef(knode->freetop) + d));
    for (i = 0; i <= hmask; i++) {
      Node *kn = &knode[i];
      Node *n = &node[i];
      Node *next = nextnode(kn);
      /* Don't use copyTV here, since it asserts on a copy of a dead key. */
      n->val = kn->val; n->key = kn->key;
      setmref(n->next, next == NULL? next : (Node *)((char *)next + d));
    }
  }
  return t;
}

/* Free a table. */
void LJ_FASTCALL lj_tab_free(global_State *g, GCtab *t)
{
  if (t->hmask > 0)
    lj_mem_freevec(g, noderef(t->node), t->hmask+1, Node);
  if (t->asize > 0 && LJ_MAX_COLOSIZE != 0 && t->colo <= 0)
    lj_mem_freevec(g, tvref(t->array), t->asize, TValue);
  if (LJ_MAX_COLOSIZE != 0 && t->colo)
    lj_mem_free(g, t, sizetabcolo((uint32_t)t->colo & 0x7f));
  else
    lj_mem_freet(g, t);
}

/* -- Table resizing ------------------------------------------------------ */

/* Resize a table to fit the new array/hash part sizes. */
static void resizetab(lua_State *L, GCtab *t, uint32_t asize, uint32_t hbits)
{
  Node *oldnode = noderef(t->node);
  uint32_t oldasize = t->asize;
  uint32_t oldhmask = t->hmask;
  if (asize > oldasize) {  /* Array part grows? */
    TValue *array;
    uint32_t i;
    if (asize > LJ_MAX_ASIZE)
      lj_err_msg(L, LJ_ERR_TABOV);
    if (LJ_MAX_COLOSIZE != 0 && t->colo > 0) {
      /* A colocated array must be separated and copied. */
      TValue *oarray = tvref(t->array);
      array = lj_mem_newvec(L, asize, TValue);
      t->colo = (int8_t)(t->colo | 0x80);  /* Mark as separated (colo < 0). */
      for (i = 0; i < oldasize; i++)
	copyTV(L, &array[i], &oarray[i]);
    } else {
      array = (TValue *)lj_mem_realloc(L, tvref(t->array),
			  oldasize*sizeof(TValue), asize*sizeof(TValue));
    }
    setmref(t->array, array);
    t->asize = asize;
    for (i = oldasize; i < asize; i++)  /* Clear newly allocated slots. */
      setnilV(&array[i]);
  }
  /* Create new (empty) hash part. */
  if (hbits) {
    newhpart(L, t, hbits);
    clearhpart(t);
  } else {
    global_State *g = G(L);
    setmref(t->node, &g->nilnode);
    t->hmask = 0;
  }
  if (asize < oldasize) {  /* Array part shrinks? */
    TValue *array = tvref(t->array);
    uint32_t i;
    t->asize = asize;  /* Note: This 'shrinks' even colocated arrays. */
    for (i = asize; i < oldasize; i++)  /* Reinsert old array values. */
      if (!tvisnil(&array[i]))
	copyTV(L, lj_tab_setinth(L, t, (int32_t)i), &array[i]);
    /* Physically shrink only separated arrays. */
    if (LJ_MAX_COLOSIZE != 0 && t->colo <= 0)
      setmref(t->array, lj_mem_realloc(L, array,
	      oldasize*sizeof(TValue), asize*sizeof(TValue)));
  }
  if (oldhmask > 0) {  /* Reinsert pairs from old hash part. */
    global_State *g;
    uint32_t i;
    for (i = 0; i <= oldhmask; i++) {
      Node *n = &oldnode[i];
      if (!tvisnil(&n->val))
	copyTV(L, lj_tab_set(L, t, &n->key), &n->val);
    }
    g = G(L);
    lj_mem_freevec(g, oldnode, oldhmask+1, Node);
  }
}

static uint32_t countint(cTValue *key, uint32_t *bins)
{
  lua_assert(!tvisint(key));
  if (tvisnum(key)) {
    lua_Number nk = numV(key);
    int32_t k = lj_num2int(nk);
    if ((uint32_t)k < LJ_MAX_ASIZE && nk == (lua_Number)k) {
      bins[(k > 2 ? lj_fls((uint32_t)(k-1)) : 0)]++;
      return 1;
    }
  }
  return 0;
}

static uint32_t countarray(const GCtab *t, uint32_t *bins)
{
  uint32_t na, b, i;
  if (t->asize == 0) return 0;
  for (na = i = b = 0; b < LJ_MAX_ABITS; b++) {
    uint32_t n, top = 2u << b;
    TValue *array;
    if (top >= t->asize) {
      top = t->asize-1;
      if (i > top)
	break;
    }
    array = tvref(t->array);
    for (n = 0; i <= top; i++)
      if (!tvisnil(&array[i]))
	n++;
    bins[b] += n;
    na += n;
  }
  return na;
}

static uint32_t counthash(const GCtab *t, uint32_t *bins, uint32_t *narray)
{
  uint32_t total, na, i, hmask = t->hmask;
  Node *node = noderef(t->node);
  for (total = na = 0, i = 0; i <= hmask; i++) {
    Node *n = &node[i];
    if (!tvisnil(&n->val)) {
      na += countint(&n->key, bins);
      total++;
    }
  }
  *narray += na;
  return total;
}

static uint32_t bestasize(uint32_t bins[], uint32_t *narray)
{
  uint32_t b, sum, na = 0, sz = 0, nn = *narray;
  for (b = 0, sum = 0; 2*nn > (1u<<b) && sum != nn; b++)
    if (bins[b] > 0 && 2*(sum += bins[b]) > (1u<<b)) {
      sz = (2u<<b)+1;
      na = sum;
    }
  *narray = sz;
  return na;
}

static void rehashtab(lua_State *L, GCtab *t, cTValue *ek)
{
  uint32_t bins[LJ_MAX_ABITS];
  uint32_t total, asize, na, i;
  for (i = 0; i < LJ_MAX_ABITS; i++) bins[i] = 0;
  asize = countarray(t, bins);
  total = 1 + asize;
  total += counthash(t, bins, &asize);
  asize += countint(ek, bins);
  na = bestasize(bins, &asize);
  total -= na;
  resizetab(L, t, asize, hsize2hbits(total));
}

void lj_tab_reasize(lua_State *L, GCtab *t, uint32_t nasize)
{
  resizetab(L, t, nasize+1, t->hmask > 0 ? lj_fls(t->hmask)+1 : 0);
}

/* -- Table getters ------------------------------------------------------- */

cTValue * LJ_FASTCALL lj_tab_getinth(GCtab *t, int32_t key)
{
  TValue k;
  Node *n;
  k.n = (lua_Number)key;
  n = hashnum(t, &k);
  do {
    if (tvisnum(&n->key) && n->key.n == k.n)
      return &n->val;
  } while ((n = nextnode(n)));
  return NULL;
}

cTValue *lj_tab_getstr(GCtab *t, GCstr *key)
{
  Node *n = hashstr(t, key);
  do {
    if (tvisstr(&n->key) && strV(&n->key) == key)
      return &n->val;
  } while ((n = nextnode(n)));
  return NULL;
}

cTValue *lj_tab_get(lua_State *L, GCtab *t, cTValue *key)
{
  if (tvisstr(key)) {
    cTValue *tv = lj_tab_getstr(t, strV(key));
    if (tv)
      return tv;
  } else if (tvisint(key)) {
    cTValue *tv = lj_tab_getint(t, intV(key));
    if (tv)
      return tv;
  } else if (tvisnum(key)) {
    lua_Number nk = numV(key);
    int32_t k = lj_num2int(nk);
    if (nk == (lua_Number)k) {
      cTValue *tv = lj_tab_getint(t, k);
      if (tv)
	return tv;
    } else {
      goto genlookup;  /* Else use the generic lookup. */
    }
  } else if (!tvisnil(key)) {
    Node *n;
  genlookup:
    n = hashkey(t, key);
    do {
      if (lj_obj_equal(&n->key, key))
	return &n->val;
    } while ((n = nextnode(n)));
  }
  return niltv(L);
}

/* -- Table setters ------------------------------------------------------- */

/* Insert new key. Use Brent's variation to optimize the chain length. */
TValue *lj_tab_newkey(lua_State *L, GCtab *t, cTValue *key)
{
  Node *n = hashkey(t, key);
  if (!tvisnil(&n->val) || t->hmask == 0) {
    Node *nodebase = noderef(t->node);
    Node *collide, *freenode = noderef(nodebase->freetop);
    lua_assert(freenode >= nodebase && freenode <= nodebase+t->hmask+1);
    do {
      if (freenode == nodebase) {  /* No free node found? */
	rehashtab(L, t, key);  /* Rehash table. */
	return lj_tab_set(L, t, key);  /* Retry key insertion. */
      }
    } while (!tvisnil(&(--freenode)->key));
    setmref(nodebase->freetop, freenode);
    lua_assert(freenode != &G(L)->nilnode);
    collide = hashkey(t, &n->key);
    if (collide != n) {  /* Colliding node not the main node? */
      while (noderef(collide->next) != n)  /* Find predecessor. */
	collide = nextnode(collide);
      setmref(collide->next, freenode);  /* Relink chain. */
      /* Copy colliding node into free node and free main node. */
      freenode->val = n->val;
      freenode->key = n->key;
      freenode->next = n->next;
      setmref(n->next, NULL);
      setnilV(&n->val);
      /* Rechain pseudo-resurrected string keys with colliding hashes. */
      while (nextnode(freenode)) {
	Node *nn = nextnode(freenode);
	if (tvisstr(&nn->key) && !tvisnil(&nn->val) &&
	    hashstr(t, strV(&nn->key)) == n) {
	  freenode->next = nn->next;
	  nn->next = n->next;
	  setmref(n->next, nn);
	} else {
	  freenode = nn;
	}
      }
    } else {  /* Otherwise use free node. */
      setmrefr(freenode->next, n->next);  /* Insert into chain. */
      setmref(n->next, freenode);
      n = freenode;
    }
  }
  n->key.u64 = key->u64;
  if (LJ_UNLIKELY(tvismzero(&n->key)))
    n->key.u64 = 0;
  lj_gc_anybarriert(L, t);
  lua_assert(tvisnil(&n->val));
  return &n->val;
}

TValue *lj_tab_setinth(lua_State *L, GCtab *t, int32_t key)
{
  TValue k;
  Node *n;
  k.n = (lua_Number)key;
  n = hashnum(t, &k);
  do {
    if (tvisnum(&n->key) && n->key.n == k.n)
      return &n->val;
  } while ((n = nextnode(n)));
  return lj_tab_newkey(L, t, &k);
}

TValue *lj_tab_setstr(lua_State *L, GCtab *t, GCstr *key)
{
  TValue k;
  Node *n = hashstr(t, key);
  do {
    if (tvisstr(&n->key) && strV(&n->key) == key)
      return &n->val;
  } while ((n = nextnode(n)));
  setstrV(L, &k, key);
  return lj_tab_newkey(L, t, &k);
}

TValue *lj_tab_set(lua_State *L, GCtab *t, cTValue *key)
{
  Node *n;
  t->nomm = 0;  /* Invalidate negative metamethod cache. */
  if (tvisstr(key)) {
    return lj_tab_setstr(L, t, strV(key));
  } else if (tvisint(key)) {
    return lj_tab_setint(L, t, intV(key));
  } else if (tvisnum(key)) {
    lua_Number nk = numV(key);
    int32_t k = lj_num2int(nk);
    if (nk == (lua_Number)k)
      return lj_tab_setint(L, t, k);
    if (tvisnan(key))
      lj_err_msg(L, LJ_ERR_NANIDX);
    /* Else use the generic lookup. */
  } else if (tvisnil(key)) {
    lj_err_msg(L, LJ_ERR_NILIDX);
  }
  n = hashkey(t, key);
  do {
    if (lj_obj_equal(&n->key, key))
      return &n->val;
  } while ((n = nextnode(n)));
  return lj_tab_newkey(L, t, key);
}

/* -- Table traversal ----------------------------------------------------- */

/* Get the traversal index of a key. */
static uint32_t keyindex(lua_State *L, GCtab *t, cTValue *key)
{
  TValue tmp;
  if (tvisint(key)) {
    int32_t k = intV(key);
    if ((uint32_t)k < t->asize)
      return (uint32_t)k;  /* Array key indexes: [0..t->asize-1] */
    setnumV(&tmp, (lua_Number)k);
    key = &tmp;
  } else if (tvisnum(key)) {
    lua_Number nk = numV(key);
    int32_t k = lj_num2int(nk);
    if ((uint32_t)k < t->asize && nk == (lua_Number)k)
      return (uint32_t)k;  /* Array key indexes: [0..t->asize-1] */
  }
  if (!tvisnil(key)) {
    Node *n = hashkey(t, key);
    do {
      if (lj_obj_equal(&n->key, key))
	return t->asize + (uint32_t)(n - noderef(t->node));
	/* Hash key indexes: [t->asize..t->asize+t->nmask] */
    } while ((n = nextnode(n)));
    if (key->u32.hi == 0xfffe7fff)  /* ITERN was despecialized while running. */
      return key->u32.lo - 1;
    lj_err_msg(L, LJ_ERR_NEXTIDX);
    return 0;  /* unreachable */
  }
  return ~0u;  /* A nil key starts the traversal. */
}

/* Advance to the next step in a table traversal. */
int lj_tab_next(lua_State *L, GCtab *t, TValue *key)
{
  uint32_t i = keyindex(L, t, key);  /* Find predecessor key index. */
  for (i++; i < t->asize; i++)  /* First traverse the array keys. */
    if (!tvisnil(arrayslot(t, i))) {
      setintV(key, i);
      copyTV(L, key+1, arrayslot(t, i));
      return 1;
    }
  for (i -= t->asize; i <= t->hmask; i++) {  /* Then traverse the hash keys. */
    Node *n = &noderef(t->node)[i];
    if (!tvisnil(&n->val)) {
      copyTV(L, key, &n->key);
      copyTV(L, key+1, &n->val);
      return 1;
    }
  }
  return 0;  /* End of traversal. */
}

/* -- Table length calculation -------------------------------------------- */

static MSize unbound_search(GCtab *t, MSize j)
{
  cTValue *tv;
  MSize i = j;  /* i is zero or a present index */
  j++;
  /* find `i' and `j' such that i is present and j is not */
  while ((tv = lj_tab_getint(t, (int32_t)j)) && !tvisnil(tv)) {
    i = j;
    j *= 2;
    if (j > (MSize)(INT_MAX-2)) {  /* overflow? */
      /* table was built with bad purposes: resort to linear search */
      i = 1;
      while ((tv = lj_tab_getint(t, (int32_t)i)) && !tvisnil(tv)) i++;
      return i - 1;
    }
  }
  /* now do a binary search between them */
  while (j - i > 1) {
    MSize m = (i+j)/2;
    cTValue *tvb = lj_tab_getint(t, (int32_t)m);
    if (tvb && !tvisnil(tvb)) i = m; else j = m;
  }
  return i;
}

/*
** Try to find a boundary in table `t'. A `boundary' is an integer index
** such that t[i] is non-nil and t[i+1] is nil (and 0 if t[1] is nil).
*/
MSize LJ_FASTCALL lj_tab_len(GCtab *t)
{
  MSize j = (MSize)t->asize;
  if (j > 1 && tvisnil(arrayslot(t, j-1))) {
    MSize i = 1;
    while (j - i > 1) {
      MSize m = (i+j)/2;
      if (tvisnil(arrayslot(t, m-1))) j = m; else i = m;
    }
    return i-1;
  }
  if (j) j--;
  if (t->hmask <= 0)
    return j;
  return unbound_search(t, j);
}

