// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --trace-ignition

// Test tracing doesn't crash or leak. Not explicitly pattern matching.
function f() {
  var values = [];
  var x = 10000000;
  var y = x + 1;
  var _aa;
  var _ab;
  var _ac;
  var _ad;
  var _ae;
  var _af;
  var _ag;
  var _ah;
  var _ai;
  var _aj;
  var _ak;
  var _al;
  var _am;
  var _an;
  var _ao;
  var _ap;
  var _ba;
  var _bb;
  var _bc;
  var _bd;
  var _be;
  var _bf;
  var _bg;
  var _bh;
  var _bi;
  var _bj;
  var _bk;
  var _bl;
  var _bm;
  var _bn;
  var _bo;
  var _bp;
  var _ca;
  var _cb;
  var _cc;
  var _cd;
  var _ce;
  var _cf;
  var _cg;
  var _ch;
  var _ci;
  var _cj;
  var _ck;
  var _cl;
  var _cm;
  var _cn;
  var _co;
  var _cp;
  var _da;
  var _db;
  var _dc;
  var _dd;
  var _de;
  var _df;
  var _dg;
  var _dh;
  var _di;
  var _dj;
  var _dk;
  var _dl;
  var _dm;
  var _dn;
  var _do;
  var _dp;
  var _ea;
  var _eb;
  var _ec;
  var _ed;
  var _ee;
  var _ef;
  var _eg;
  var _eh;
  var _ei;
  var _ej;
  var _ek;
  var _el;
  var _em;
  var _en;
  var _eo;
  var _ep;
  var _fa;
  var _fb;
  var _fc;
  var _fd;
  var _fe;
  var _ff;
  var _fg;
  var _fh;
  var _fi;
  var _fj;
  var _fk;
  var _fl;
  var _fm;
  var _fn;
  var _fo;
  var _fp;
  var _ga;
  var _gb;
  var _gc;
  var _gd;
  var _ge;
  var _gf;
  var _gg;
  var _gh;
  var _gi;
  var _gj;
  var _gk;
  var _gl;
  var _gm;
  var _gn;
  var _go;
  var _gp;
  var _ha;
  var _hb;
  var _hc;
  var _hd;
  var _he;
  var _hf;
  var _hg;
  var _hh;
  var _hi;
  var _hj;
  var _hk;
  var _hl;
  var _hm;
  var _hn;
  var _ho;
  var _hp;
  var _ia;
  var _ib;
  var _ic;
  var _id;
  var _ie;
  var _if;
  var _ig;
  var _ih;
  var _ii;
  var _ij;
  var _ik;
  var _il;
  var _im;
  var _in;
  var _io;
  var _ip;
  var _ja;
  var _jb;
  var _jc;
  var _jd;
  var _je;
  var _jf;
  var _jg;
  var _jh;
  var _ji;
  var _jj;
  var _jk;
  var _jl;
  var _jm;
  var _jn;
  var _jo;
  var _jp;
  var _ka;
  var _kb;
  var _kc;
  var _kd;
  var _ke;
  var _kf;
  var _kg;
  var _kh;
  var _ki;
  var _kj;
  var _kk;
  var _kl;
  var _km;
  var _kn;
  var _ko;
  var _kp;
  var _la;
  var _lb;
  var _lc;
  var _ld;
  var _le;
  var _lf;
  var _lg;
  var _lh;
  var _li;
  var _lj;
  var _lk;
  var _ll;
  var _lm;
  var _ln;
  var _lo;
  var _lp;
  var _ma;
  var _mb;
  var _mc;
  var _md;
  var _me;
  var _mf;
  var _mg;
  var _mh;
  var _mi;
  var _mj;
  var _mk;
  var _ml;
  var _mm;
  var _mn;
  var _mo;
  var _mp;
  var _na;
  var _nb;
  var _nc;
  var _nd;
  var _ne;
  var _nf;
  var _ng;
  var _nh;
  var _ni;
  var _nj;
  var _nk;
  var _nl;
  var _nm;
  var _nn;
  var _no;
  var _np;
  var _oa;
  var _ob;
  var _oc;
  var _od;
  var _oe;
  var _of;
  var _og;
  var _oh;
  var _oi;
  var _oj;
  var _ok;
  var _ol;
  var _om;
  var _on;
  var _oo;
  var _op;
  var _pa;
  var _pb;
  var _pc;
  var _pd;
  var _pe;
  var _pf;
  var _pg;
  var _ph;
  var _pi;
  var _pj;
  var _pk;
  var _pl;
  var _pm;
  var _pn;
  var _po;
  var _pp;
  var _qa;
  var _qb;
  var _qc;
  var _qd;
  var _qe;
  var _qf;
  var _qg;
  var _qh;
  var _qi;
  var _qj;
  var _qk;
  var _ql;
  var _qm;
  var _qn;
  var _qo;
  var _qp;
  var _ra;
  var _rb;
  var _rc;
  var _rd;
  var _re;
  var _rf;
  var _rg;
  var _rh;
  var _ri;
  var _rj;
  var _rk;
  var _rl;
  var _rm;
  var _rn;
  var _ro;
  var _rp = 287; values[_rp] = _rp;
};

f();
