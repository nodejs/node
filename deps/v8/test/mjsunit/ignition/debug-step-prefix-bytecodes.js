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

// Flags: --ignition-filter=f --expose-debug-as debug

// This test tests that full code compiled without debug break slots
// is recompiled with debug break slots when debugging is started.

// Get the Debug object exposed from the debug context global object.
Debug = debug.Debug

var done = false;
var step_count = 0;
var values = [];

// Debug event listener which steps until the global variable done is true.
function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) {
    if (!done) exec_state.prepareStep(Debug.StepAction.StepNext);
    step_count++;
  }
};

// Set the global variables state to prpare the stepping test.
function prepare_step_test() {
  done = false;
  step_count = 0;
}

// Test function to step through, uses widended bytecodes.
function f() {
var x = 10000000;
var y = x + 1;
var _aa = 0; values[_aa] = _aa;
var _ab = 1; values[_ab] = _ab;
var _ac = 2; values[_ac] = _ac;
var _ad = 3; values[_ad] = _ad;
var _ae = 4; values[_ae] = _ae;
var _af = 5; values[_af] = _af;
var _ag = 6; values[_ag] = _ag;
var _ah = 7; values[_ah] = _ah;
var _ai = 8; values[_ai] = _ai;
var _aj = 9; values[_aj] = _aj;
var _ak = 10; values[_ak] = _ak;
var _al = 11; values[_al] = _al;
var _am = 12; values[_am] = _am;
var _an = 13; values[_an] = _an;
var _ao = 14; values[_ao] = _ao;
var _ap = 15; values[_ap] = _ap;
var _ba = 16; values[_ba] = _ba;
var _bb = 17; values[_bb] = _bb;
var _bc = 18; values[_bc] = _bc;
var _bd = 19; values[_bd] = _bd;
var _be = 20; values[_be] = _be;
var _bf = 21; values[_bf] = _bf;
var _bg = 22; values[_bg] = _bg;
var _bh = 23; values[_bh] = _bh;
var _bi = 24; values[_bi] = _bi;
var _bj = 25; values[_bj] = _bj;
var _bk = 26; values[_bk] = _bk;
var _bl = 27; values[_bl] = _bl;
var _bm = 28; values[_bm] = _bm;
var _bn = 29; values[_bn] = _bn;
var _bo = 30; values[_bo] = _bo;
var _bp = 31; values[_bp] = _bp;
var _ca = 32; values[_ca] = _ca;
var _cb = 33; values[_cb] = _cb;
var _cc = 34; values[_cc] = _cc;
var _cd = 35; values[_cd] = _cd;
var _ce = 36; values[_ce] = _ce;
var _cf = 37; values[_cf] = _cf;
var _cg = 38; values[_cg] = _cg;
var _ch = 39; values[_ch] = _ch;
var _ci = 40; values[_ci] = _ci;
var _cj = 41; values[_cj] = _cj;
var _ck = 42; values[_ck] = _ck;
var _cl = 43; values[_cl] = _cl;
var _cm = 44; values[_cm] = _cm;
var _cn = 45; values[_cn] = _cn;
var _co = 46; values[_co] = _co;
var _cp = 47; values[_cp] = _cp;
var _da = 48; values[_da] = _da;
var _db = 49; values[_db] = _db;
var _dc = 50; values[_dc] = _dc;
var _dd = 51; values[_dd] = _dd;
var _de = 52; values[_de] = _de;
var _df = 53; values[_df] = _df;
var _dg = 54; values[_dg] = _dg;
var _dh = 55; values[_dh] = _dh;
var _di = 56; values[_di] = _di;
var _dj = 57; values[_dj] = _dj;
var _dk = 58; values[_dk] = _dk;
var _dl = 59; values[_dl] = _dl;
var _dm = 60; values[_dm] = _dm;
var _dn = 61; values[_dn] = _dn;
var _do = 62; values[_do] = _do;
var _dp = 63; values[_dp] = _dp;
var _ea = 64; values[_ea] = _ea;
var _eb = 65; values[_eb] = _eb;
var _ec = 66; values[_ec] = _ec;
var _ed = 67; values[_ed] = _ed;
var _ee = 68; values[_ee] = _ee;
var _ef = 69; values[_ef] = _ef;
var _eg = 70; values[_eg] = _eg;
var _eh = 71; values[_eh] = _eh;
var _ei = 72; values[_ei] = _ei;
var _ej = 73; values[_ej] = _ej;
var _ek = 74; values[_ek] = _ek;
var _el = 75; values[_el] = _el;
var _em = 76; values[_em] = _em;
var _en = 77; values[_en] = _en;
var _eo = 78; values[_eo] = _eo;
var _ep = 79; values[_ep] = _ep;
var _fa = 80; values[_fa] = _fa;
var _fb = 81; values[_fb] = _fb;
var _fc = 82; values[_fc] = _fc;
var _fd = 83; values[_fd] = _fd;
var _fe = 84; values[_fe] = _fe;
var _ff = 85; values[_ff] = _ff;
var _fg = 86; values[_fg] = _fg;
var _fh = 87; values[_fh] = _fh;
var _fi = 88; values[_fi] = _fi;
var _fj = 89; values[_fj] = _fj;
var _fk = 90; values[_fk] = _fk;
var _fl = 91; values[_fl] = _fl;
var _fm = 92; values[_fm] = _fm;
var _fn = 93; values[_fn] = _fn;
var _fo = 94; values[_fo] = _fo;
var _fp = 95; values[_fp] = _fp;
var _ga = 96; values[_ga] = _ga;
var _gb = 97; values[_gb] = _gb;
var _gc = 98; values[_gc] = _gc;
var _gd = 99; values[_gd] = _gd;
var _ge = 100; values[_ge] = _ge;
var _gf = 101; values[_gf] = _gf;
var _gg = 102; values[_gg] = _gg;
var _gh = 103; values[_gh] = _gh;
var _gi = 104; values[_gi] = _gi;
var _gj = 105; values[_gj] = _gj;
var _gk = 106; values[_gk] = _gk;
var _gl = 107; values[_gl] = _gl;
var _gm = 108; values[_gm] = _gm;
var _gn = 109; values[_gn] = _gn;
var _go = 110; values[_go] = _go;
var _gp = 111; values[_gp] = _gp;
var _ha = 112; values[_ha] = _ha;
var _hb = 113; values[_hb] = _hb;
var _hc = 114; values[_hc] = _hc;
var _hd = 115; values[_hd] = _hd;
var _he = 116; values[_he] = _he;
var _hf = 117; values[_hf] = _hf;
var _hg = 118; values[_hg] = _hg;
var _hh = 119; values[_hh] = _hh;
var _hi = 120; values[_hi] = _hi;
var _hj = 121; values[_hj] = _hj;
var _hk = 122; values[_hk] = _hk;
var _hl = 123; values[_hl] = _hl;
var _hm = 124; values[_hm] = _hm;
var _hn = 125; values[_hn] = _hn;
var _ho = 126; values[_ho] = _ho;
var _hp = 127; values[_hp] = _hp;
var _ia = 128; values[_ia] = _ia;
var _ib = 129; values[_ib] = _ib;
var _ic = 130; values[_ic] = _ic;
var _id = 131; values[_id] = _id;
var _ie = 132; values[_ie] = _ie;
var _if = 133; values[_if] = _if;
var _ig = 134; values[_ig] = _ig;
var _ih = 135; values[_ih] = _ih;
var _ii = 136; values[_ii] = _ii;
var _ij = 137; values[_ij] = _ij;
var _ik = 138; values[_ik] = _ik;
var _il = 139; values[_il] = _il;
var _im = 140; values[_im] = _im;
var _in = 141; values[_in] = _in;
var _io = 142; values[_io] = _io;
var _ip = 143; values[_ip] = _ip;
var _ja = 144; values[_ja] = _ja;
var _jb = 145; values[_jb] = _jb;
var _jc = 146; values[_jc] = _jc;
var _jd = 147; values[_jd] = _jd;
var _je = 148; values[_je] = _je;
var _jf = 149; values[_jf] = _jf;
var _jg = 150; values[_jg] = _jg;
var _jh = 151; values[_jh] = _jh;
var _ji = 152; values[_ji] = _ji;
var _jj = 153; values[_jj] = _jj;
var _jk = 154; values[_jk] = _jk;
var _jl = 155; values[_jl] = _jl;
var _jm = 156; values[_jm] = _jm;
var _jn = 157; values[_jn] = _jn;
var _jo = 158; values[_jo] = _jo;
var _jp = 159; values[_jp] = _jp;
var _ka = 160; values[_ka] = _ka;
var _kb = 161; values[_kb] = _kb;
var _kc = 162; values[_kc] = _kc;
var _kd = 163; values[_kd] = _kd;
var _ke = 164; values[_ke] = _ke;
var _kf = 165; values[_kf] = _kf;
var _kg = 166; values[_kg] = _kg;
var _kh = 167; values[_kh] = _kh;
var _ki = 168; values[_ki] = _ki;
var _kj = 169; values[_kj] = _kj;
var _kk = 170; values[_kk] = _kk;
var _kl = 171; values[_kl] = _kl;
var _km = 172; values[_km] = _km;
var _kn = 173; values[_kn] = _kn;
var _ko = 174; values[_ko] = _ko;
var _kp = 175; values[_kp] = _kp;
var _la = 176; values[_la] = _la;
var _lb = 177; values[_lb] = _lb;
var _lc = 178; values[_lc] = _lc;
var _ld = 179; values[_ld] = _ld;
var _le = 180; values[_le] = _le;
var _lf = 181; values[_lf] = _lf;
var _lg = 182; values[_lg] = _lg;
var _lh = 183; values[_lh] = _lh;
var _li = 184; values[_li] = _li;
var _lj = 185; values[_lj] = _lj;
var _lk = 186; values[_lk] = _lk;
var _ll = 187; values[_ll] = _ll;
var _lm = 188; values[_lm] = _lm;
var _ln = 189; values[_ln] = _ln;
var _lo = 190; values[_lo] = _lo;
var _lp = 191; values[_lp] = _lp;
var _ma = 192; values[_ma] = _ma;
var _mb = 193; values[_mb] = _mb;
var _mc = 194; values[_mc] = _mc;
var _md = 195; values[_md] = _md;
var _me = 196; values[_me] = _me;
var _mf = 197; values[_mf] = _mf;
var _mg = 198; values[_mg] = _mg;
var _mh = 199; values[_mh] = _mh;
var _mi = 200; values[_mi] = _mi;
var _mj = 201; values[_mj] = _mj;
var _mk = 202; values[_mk] = _mk;
var _ml = 203; values[_ml] = _ml;
var _mm = 204; values[_mm] = _mm;
var _mn = 205; values[_mn] = _mn;
var _mo = 206; values[_mo] = _mo;
var _mp = 207; values[_mp] = _mp;
var _na = 208; values[_na] = _na;
var _nb = 209; values[_nb] = _nb;
var _nc = 210; values[_nc] = _nc;
var _nd = 211; values[_nd] = _nd;
var _ne = 212; values[_ne] = _ne;
var _nf = 213; values[_nf] = _nf;
var _ng = 214; values[_ng] = _ng;
var _nh = 215; values[_nh] = _nh;
var _ni = 216; values[_ni] = _ni;
var _nj = 217; values[_nj] = _nj;
var _nk = 218; values[_nk] = _nk;
var _nl = 219; values[_nl] = _nl;
var _nm = 220; values[_nm] = _nm;
var _nn = 221; values[_nn] = _nn;
var _no = 222; values[_no] = _no;
var _np = 223; values[_np] = _np;
var _oa = 224; values[_oa] = _oa;
var _ob = 225; values[_ob] = _ob;
var _oc = 226; values[_oc] = _oc;
var _od = 227; values[_od] = _od;
var _oe = 228; values[_oe] = _oe;
var _of = 229; values[_of] = _of;
var _og = 230; values[_og] = _og;
var _oh = 231; values[_oh] = _oh;
var _oi = 232; values[_oi] = _oi;
var _oj = 233; values[_oj] = _oj;
var _ok = 234; values[_ok] = _ok;
var _ol = 235; values[_ol] = _ol;
var _om = 236; values[_om] = _om;
var _on = 237; values[_on] = _on;
var _oo = 238; values[_oo] = _oo;
var _op = 239; values[_op] = _op;
var _pa = 240; values[_pa] = _pa;
var _pb = 241; values[_pb] = _pb;
var _pc = 242; values[_pc] = _pc;
var _pd = 243; values[_pd] = _pd;
var _pe = 244; values[_pe] = _pe;
var _pf = 245; values[_pf] = _pf;
var _pg = 246; values[_pg] = _pg;
var _ph = 247; values[_ph] = _ph;
var _pi = 248; values[_pi] = _pi;
var _pj = 249; values[_pj] = _pj;
var _pk = 250; values[_pk] = _pk;
var _pl = 251; values[_pl] = _pl;
var _pm = 252; values[_pm] = _pm;
var _pn = 253; values[_pn] = _pn;
var _po = 254; values[_po] = _po;
var _pp = 255; values[_pp] = _pp;
var _qa = 256; values[_qa] = _qa;
var _qb = 257; values[_qb] = _qb;
var _qc = 258; values[_qc] = _qc;
var _qd = 259; values[_qd] = _qd;
var _qe = 260; values[_qe] = _qe;
var _qf = 261; values[_qf] = _qf;
var _qg = 262; values[_qg] = _qg;
var _qh = 263; values[_qh] = _qh;
var _qi = 264; values[_qi] = _qi;
var _qj = 265; values[_qj] = _qj;
var _qk = 266; values[_qk] = _qk;
var _ql = 267; values[_ql] = _ql;
var _qm = 268; values[_qm] = _qm;
var _qn = 269; values[_qn] = _qn;
var _qo = 270; values[_qo] = _qo;
var _qp = 271; values[_qp] = _qp;
var _ra = 272; values[_ra] = _ra;
var _rb = 273; values[_rb] = _rb;
var _rc = 274; values[_rc] = _rc;
var _rd = 275; values[_rd] = _rd;
var _re = 276; values[_re] = _re;
var _rf = 277; values[_rf] = _rf;
var _rg = 278; values[_rg] = _rg;
var _rh = 279; values[_rh] = _rh;
var _ri = 280; values[_ri] = _ri;
var _rj = 281; values[_rj] = _rj;
var _rk = 282; values[_rk] = _rk;
var _rl = 283; values[_rl] = _rl;
var _rm = 284; values[_rm] = _rm;
var _rn = 285; values[_rn] = _rn;
var _ro = 286; values[_ro] = _ro;
var _rp = 287; values[_rp] = _rp;
  done = true;
};

function check_values() {
  for (var i = 0; i < values.length; i++) {
    assertEquals(values[i], i);
    values[i] = -i;
  }
}

// Pass 1 - no debugger, no steps seen
prepare_step_test();
f();
check_values();
assertEquals(0, step_count);

// Pass 2 - debugger attached and stepping from BP
Debug.setListener(listener);
var bp = Debug.setBreakPoint(f, 1);
prepare_step_test();
f();
check_values();
assertEquals(580, step_count);
Debug.clearBreakPoint(bp);

// Pass 3 - debugger attached and no BP
prepare_step_test();
f();
check_values();
assertEquals(0, step_count);
