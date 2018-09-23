; This Emacs Lisp file defines a C indentation style that closely
; follows most aspects of the one that is used throughout SSLeay,
; and hence in OpenSSL.
; 
; This definition is for the "CC mode" package, which is the default
; mode for editing C source files in Emacs 20, not for the older
; c-mode.el (which was the default in less recent releaes of Emacs 19).
;
; Copy the definition in your .emacs file or use M-x eval-buffer.
; To activate this indentation style, visit a C file, type
; M-x c-set-style <RET> (or C-c . for short), and enter "eay".
; To toggle the auto-newline feature of CC mode, type C-c C-a.
;
; Apparently statement blocks that are not introduced by a statement
; such as "if" and that are not the body of a function cannot
; be handled too well by CC mode with this indentation style,
; so you have to indent them manually (you can use C-q tab).
; 
; For suggesting improvements, please send e-mail to bodo@openssl.org.

(c-add-style "eay"
	     '((c-basic-offset . 8)
	       (indent-tabs-mode . t)
	       (c-comment-only-line-offset . 0)
	       (c-hanging-braces-alist)
	       (c-offsets-alist	. ((defun-open . +)
				   (defun-block-intro . 0)
				   (class-open . +)
				   (class-close . +)
				   (block-open . 0)
				   (block-close . 0)
				   (substatement-open . +)
				   (statement . 0)
				   (statement-block-intro . 0)
				   (statement-case-open . +)
				   (statement-case-intro . +)
				   (case-label . -)
				   (label . -)
				   (arglist-cont-nonempty . +)
				   (topmost-intro . -)
				   (brace-list-close . 0)
				   (brace-list-intro . 0)
				   (brace-list-open . +)
				   ))))

