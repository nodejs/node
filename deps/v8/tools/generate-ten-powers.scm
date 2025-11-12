;; Copyright 2010 the V8 project authors. All rights reserved.
;; Redistribution and use in source and binary forms, with or without
;; modification, are permitted provided that the following conditions are
;; met:
;;
;;     * Redistributions of source code must retain the above copyright
;;       notice, this list of conditions and the following disclaimer.
;;     * Redistributions in binary form must reproduce the above
;;       copyright notice, this list of conditions and the following
;;       disclaimer in the documentation and/or other materials provided
;;       with the distribution.
;;     * Neither the name of Google Inc. nor the names of its
;;       contributors may be used to endorse or promote products derived
;;       from this software without specific prior written permission.
;;
;; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
;; "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
;; LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
;; A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
;; OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
;; SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
;; LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
;; DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
;; THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
;; (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
;; OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

;; This is a Scheme script for the Bigloo compiler. Bigloo must be compiled with
;; support for bignums. The compilation of the script can be done as follows:
;;   bigloo -static-bigloo -o generate-ten-powers generate-ten-powers.scm
;;  
;; Generate approximations of 10^k.

(module gen-ten-powers
   (static (class Cached-Fast
	      v::bignum
	      e::bint
	      exact?::bool))
   (main my-main))


;;----------------bignum shifts -----------------------------------------------
(define (bit-lshbx::bignum x::bignum by::bint)
   (if (<fx by 0)
       #z0
       (*bx x (exptbx #z2 (fixnum->bignum by)))))

(define (bit-rshbx::bignum x::bignum by::bint)
   (if (<fx by 0)
       #z0
       (/bx x (exptbx #z2 (fixnum->bignum by)))))

;;----------------the actual power generation -------------------------------

;; e should be an indication. it might be too small.
(define (round-n-cut n e nb-bits)
   (define max-container (- (bit-lshbx #z1 nb-bits) 1))
   (define (round n)
      (case *round*
	 ((down) n)
	 ((up)
	  (+bx n
	       ;; with the -1 it will only round up if the cut off part is
	       ;; non-zero
	       (-bx (bit-lshbx #z1
			       (-fx (+fx e nb-bits) 1))
		    #z1)))
	 ((round)
	  (+bx n
	       (bit-lshbx #z1
			  (-fx (+fx e nb-bits) 2))))))
   (let* ((shift (-fx (+fx e nb-bits) 1))
	  (cut (bit-rshbx (round n) shift))
	  (exact? (=bx n (bit-lshbx cut shift))))
      (if (<=bx cut max-container)
	  (values cut e exact?)
	  (round-n-cut n (+fx e 1) nb-bits))))

(define (rounded-/bx x y)
   (case *round*
      ((down)  (/bx x y))
      ((up)    (+bx (/bx x y) #z1))
      ((round) (let ((tmp (/bx (*bx #z2 x) y)))
		  (if (zerobx? (remainderbx tmp #z2))
		      (/bx tmp #z2)
		      (+bx (/bx tmp #z2) #z1))))))

(define (generate-powers from to mantissa-size)
   (let* ((nb-bits mantissa-size)
	  (offset (- from))
	  (nb-elements (+ (- from) to 1))
	  (vec (make-vector nb-elements))
	  (max-container (- (bit-lshbx #z1 nb-bits) 1)))
      ;; the negative ones. 10^-1, 10^-2, etc.
      ;; We already know, that we can't be exact, so exact? will always be #f.
      ;; Basically we will have a ten^i that we will *10 at each iteration. We
      ;; want to create the matissa of 1/ten^i. However the mantissa must be
      ;; normalized (start with a 1). -> we have to shift the number.
      ;; We shift by multiplying with two^e. -> We encode two^e*(1/ten^i) ==
      ;;  two^e/ten^i.
      (let loop ((i 1)
		 (ten^i #z10)
		 (two^e #z1)
		 (e 0))
	 (unless (< (- i) from)
	    (if (>bx (/bx (*bx #z2 two^e) ten^i) max-container)
		;; another shift would make the number too big. We are
		;; hence normalized now.
		(begin
		   (vector-set! vec (-fx offset i)
				(instantiate::Cached-Fast
				   (v (rounded-/bx two^e ten^i))
				   (e (negfx e))
				   (exact? #f)))
		   (loop (+fx i 1) (*bx ten^i #z10) two^e e))
		(loop i ten^i (bit-lshbx two^e 1) (+fx e 1)))))
      ;; the positive ones 10^0, 10^1, etc.
      ;; start with 1.0. mantissa: 10...0 (1 followed by nb-bits-1 bits)
      ;;      -> e = -(nb-bits-1)
      ;; exact? is true when the container can still hold the complete 10^i
      (let loop ((i 0)
		 (n (bit-lshbx #z1 (-fx nb-bits 1)))
		 (e (-fx 1 nb-bits)))
	 (when (<= i to)
	    (receive (cut e exact?)
	       (round-n-cut n e nb-bits)
	       (vector-set! vec (+fx i offset)
			    (instantiate::Cached-Fast
			       (v cut)
			       (e e)
			       (exact? exact?)))
	       (loop (+fx i 1) (*bx n #z10) e))))
      vec))

(define (print-c powers from to struct-type
		 cache-name max-distance-name offset-name macro64)
   (define (display-power power k)
      (with-access::Cached-Fast power (v e exact?)
	 (let ((tmp-p (open-output-string)))
	    ;; really hackish way of getting the digits
	    (display (format "~x" v) tmp-p)
	    (let ((str (close-output-port tmp-p)))
	       (printf "  {~a(0x~a, ~a), ~a, ~a},\n"
		       macro64
		       (substring str 0 8)
		       (substring str 8 16)
		       e
		       k)))))
   (define (print-powers-reduced n)
      (print "static const " struct-type " " cache-name
	     "(" n ")"
	     "[] = {")
      (let loop ((i 0)
		 (nb-elements 0)
		 (last-e 0)
		 (max-distance 0))
	 (cond
	    ((>= i (vector-length powers))
	     (print "  };")
	     (print "static const int " max-distance-name "(" n ") = "
		 max-distance ";")
	     (print "// nb elements (" n "): " nb-elements))
	    (else
	     (let* ((power (vector-ref powers i))
		    (e (Cached-Fast-e power)))
	     (display-power power (+ i from))
	     (loop (+ i n)
		   (+ nb-elements 1)
		   e
		   (cond
		      ((=fx i 0) max-distance)
		      ((> (- e last-e) max-distance) (- e last-e))
		      (else max-distance))))))))
   (print "// Copyright 2010 the V8 project authors. All rights reserved.")
   (print "// ------------ GENERATED FILE ----------------")
   (print "// command used:")
   (print "// "
	  (apply string-append (map (lambda (str)
				       (string-append " " str))
				    *main-args*))
	  "  // NOLINT")
   (print)
   (print
    "// This file is intended to be included inside another .h or .cc files\n"
    "// with the following defines set:\n"
    "//  GRISU_CACHE_STRUCT: should expand to the name of a struct that will\n"
    "//   hold the cached powers of ten. Each entry will hold a 64-bit\n"
    "//   significand, a 16-bit signed binary exponent, and a 16-bit\n"
    "//   signed decimal exponent. Each entry will be constructed as follows:\n"
    "//      { significand, binary_exponent, decimal_exponent }.\n"
    "//  GRISU_CACHE_NAME(i): generates the name for the different caches.\n"
    "//   The parameter i will be a number in the range 1-20. A cache will\n"
    "//   hold every i'th element of a full cache. GRISU_CACHE_NAME(1) will\n"
    "//   thus hold all elements. The higher i the fewer elements it has.\n"
    "//   Ideally the user should only reference one cache and let the\n"
    "//   compiler remove the unused ones.\n"
    "//  GRISU_CACHE_MAX_DISTANCE(i): generates the name for the maximum\n"
    "//   binary exponent distance between all elements of a given cache.\n"
    "//  GRISU_CACHE_OFFSET: is used as variable name for the decimal\n"
    "//   exponent offset. It is equal to -cache[0].decimal_exponent.\n"
    "//  GRISU_UINT64_C: used to construct 64-bit values in a platform\n"
    "//   independent way. In order to encode 0x123456789ABCDEF0 the macro\n"
    "//   will be invoked as follows: GRISU_UINT64_C(0x12345678,9ABCDEF0).\n")
   (print)
   (print-powers-reduced 1)
   (print-powers-reduced 2)
   (print-powers-reduced 3)
   (print-powers-reduced 4)
   (print-powers-reduced 5)
   (print-powers-reduced 6)
   (print-powers-reduced 7)
   (print-powers-reduced 8)
   (print-powers-reduced 9)
   (print-powers-reduced 10)
   (print-powers-reduced 11)
   (print-powers-reduced 12)
   (print-powers-reduced 13)
   (print-powers-reduced 14)
   (print-powers-reduced 15)
   (print-powers-reduced 16)
   (print-powers-reduced 17)
   (print-powers-reduced 18)
   (print-powers-reduced 19)
   (print-powers-reduced 20)
   (print "static const int GRISU_CACHE_OFFSET = " (- from) ";"))

;;----------------main --------------------------------------------------------
(define *main-args* #f)
(define *mantissa-size* #f)
(define *dest* #f)
(define *round* #f)
(define *from* #f)
(define *to* #f)

(define (my-main args)
   (set! *main-args* args)
   (args-parse (cdr args)
      (section "Help")
      (("?") (args-parse-usage #f))
      ((("-h" "--help") (help "?, -h, --help" "This help message"))
       (args-parse-usage #f))
      (section "Misc")
      (("-o" ?file (help "The output file"))
       (set! *dest* file))
      (("--mantissa-size" ?size (help "Container-size in bits"))
       (set! *mantissa-size* (string->number size)))
      (("--round" ?direction (help "Round bignums (down, round or up)"))
       (set! *round* (string->symbol direction)))
      (("--from" ?from (help "start at 10^from"))
       (set! *from* (string->number from)))
      (("--to" ?to (help "go up to 10^to"))
       (set! *to* (string->number to)))
      (else
       (print "Illegal argument `" else "'. Usage:")
       (args-parse-usage #f)))
   (when (not *from*)
      (error "generate-ten-powers"
	     "Missing from"
	     #f))
   (when (not *to*)
      (error "generate-ten-powers"
	     "Missing to"
	     #f))
   (when (not *mantissa-size*)
      (error "generate-ten-powers"
	     "Missing mantissa size"
	     #f))
   (when (not (memv *round* '(up down round)))
      (error "generate-ten-powers"
	     "Missing round-method"
	     *round*))

   (let ((dividers (generate-powers *from* *to* *mantissa-size*))
	 (p (if (not *dest*)
		(current-output-port)
		(open-output-file *dest*))))
      (unwind-protect
	 (with-output-to-port p
	    (lambda ()
	       (print-c dividers *from* *to*
			"GRISU_CACHE_STRUCT" "GRISU_CACHE_NAME"
			"GRISU_CACHE_MAX_DISTANCE" "GRISU_CACHE_OFFSET"
			"GRISU_UINT64_C"
			)))
	 (if *dest*
	     (close-output-port p)))))
