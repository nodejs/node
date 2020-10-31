;;; protobuf-mode.el --- major mode for editing protocol buffers.

;; Author: Alexandre Vassalotti <alexandre@peadrop.com>
;; Created: 23-Apr-2009
;; Version: 0.3
;; Keywords: google protobuf languages

;; Redistribution and use in source and binary forms, with or without
;; modification, are permitted provided that the following conditions are
;; met:
;;
;;     * Redistributions of source code must retain the above copyright
;; notice, this list of conditions and the following disclaimer.
;;     * Redistributions in binary form must reproduce the above
;; copyright notice, this list of conditions and the following disclaimer
;; in the documentation and/or other materials provided with the
;; distribution.
;;     * Neither the name of Google Inc. nor the names of its
;; contributors may be used to endorse or promote products derived from
;; this software without specific prior written permission.
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

;;; Commentary:

;; Installation:
;;   - Put `protobuf-mode.el' in your Emacs load-path.
;;   - Add this line to your .emacs file:
;;       (require 'protobuf-mode)
;;
;; You can customize this mode just like any mode derived from CC Mode.  If
;; you want to add customizations specific to protobuf-mode, you can use the
;; `protobuf-mode-hook'. For example, the following would make protocol-mode
;; use 2-space indentation:
;;
;;   (defconst my-protobuf-style
;;     '((c-basic-offset . 2)
;;       (indent-tabs-mode . nil)))
;;
;;   (add-hook 'protobuf-mode-hook
;;     (lambda () (c-add-style "my-style" my-protobuf-style t)))
;;
;; Refer to the documentation of CC Mode for more information about
;; customization details and how to use this mode.
;;
;; TODO:
;;   - Make highlighting for enum values work properly.
;;   - Fix the parser to recognize extensions as identifiers and not
;;     as casts.
;;   - Improve the parsing of option assignment lists. For example:
;;       optional int32 foo = 1 [(my_field_option) = 4.5];
;;   - Add support for fully-qualified identifiers (e.g., with a leading ".").

;;; Code:

(require 'cc-mode)

(eval-when-compile
  (and (= emacs-major-version 24)
       (>= emacs-minor-version 4)
       (require 'cl))
  (require 'cc-langs)
  (require 'cc-fonts))

;; This mode does not inherit properties from other modes. So, we do not use
;; the usual `c-add-language' function.
(eval-and-compile
  (put 'protobuf-mode 'c-mode-prefix "protobuf-"))

;; The following code uses of the `c-lang-defconst' macro define syntactic
;; features of protocol buffer language.  Refer to the documentation in the
;; cc-langs.el file for information about the meaning of the -kwds variables.

(c-lang-defconst c-primitive-type-kwds
  protobuf '("double" "float" "int32" "int64" "uint32" "uint64" "sint32"
             "sint64" "fixed32" "fixed64" "sfixed32" "sfixed64" "bool"
             "string" "bytes" "group"))

(c-lang-defconst c-modifier-kwds
  protobuf '("required" "optional" "repeated"))

(c-lang-defconst c-class-decl-kwds
  protobuf '("message" "enum" "service"))

(c-lang-defconst c-constant-kwds
  protobuf '("true" "false"))

(c-lang-defconst c-other-decl-kwds
  protobuf '("package" "import"))

(c-lang-defconst c-other-kwds
  protobuf '("default" "max"))

(c-lang-defconst c-identifier-ops
  ;; Handle extended identifiers like google.protobuf.MessageOptions
  protobuf '((left-assoc ".")))

;; The following keywords do not fit well in keyword classes defined by
;; cc-mode.  So, we approximate as best we can.

(c-lang-defconst c-type-list-kwds
  protobuf '("extensions" "to" "reserved"))

(c-lang-defconst c-typeless-decl-kwds
  protobuf '("extend" "rpc" "option" "returns"))


;; Here we remove default syntax for loops, if-statements and other C
;; syntactic features that are not supported by the protocol buffer language.

(c-lang-defconst c-brace-list-decl-kwds
  ;; Remove syntax for C-style enumerations.
  protobuf nil)

(c-lang-defconst c-block-stmt-1-kwds
  ;; Remove syntax for "do" and "else" keywords.
  protobuf nil)

(c-lang-defconst c-block-stmt-2-kwds
  ;; Remove syntax for "for", "if", "switch" and "while" keywords.
  protobuf nil)

(c-lang-defconst c-simple-stmt-kwds
  ;; Remove syntax for "break", "continue", "goto" and "return" keywords.
  protobuf nil)

(c-lang-defconst c-paren-stmt-kwds
  ;; Remove special case for the "(;;)" in for-loops.
  protobuf nil)

(c-lang-defconst c-label-kwds
  ;; Remove case label syntax for the "case" and "default" keywords.
  protobuf nil)

(c-lang-defconst c-before-label-kwds
  ;; Remove special case for the label in a goto statement.
  protobuf nil)

(c-lang-defconst c-cpp-matchers
  ;; Disable all the C preprocessor syntax.
  protobuf nil)

(c-lang-defconst c-decl-prefix-re
  ;; Same as for C, except it does not match "(". This is needed for disabling
  ;; the syntax for casts.
  protobuf "\\([\{\};,]+\\)")


;; Add support for variable levels of syntax highlighting.

(defconst protobuf-font-lock-keywords-1 (c-lang-const c-matchers-1 protobuf)
  "Minimal highlighting for protobuf-mode.")

(defconst protobuf-font-lock-keywords-2 (c-lang-const c-matchers-2 protobuf)
  "Fast normal highlighting for protobuf-mode.")

(defconst protobuf-font-lock-keywords-3 (c-lang-const c-matchers-3 protobuf)
  "Accurate normal highlighting for protobuf-mode.")

(defvar protobuf-font-lock-keywords protobuf-font-lock-keywords-3
  "Default expressions to highlight in protobuf-mode.")

;; Our syntax table is auto-generated from the keyword classes we defined
;; previously with the `c-lang-const' macro.
(defvar protobuf-mode-syntax-table nil
  "Syntax table used in protobuf-mode buffers.")
(or protobuf-mode-syntax-table
    (setq protobuf-mode-syntax-table
          (funcall (c-lang-const c-make-mode-syntax-table protobuf))))

(defvar protobuf-mode-abbrev-table nil
  "Abbreviation table used in protobuf-mode buffers.")

(defvar protobuf-mode-map nil
  "Keymap used in protobuf-mode buffers.")
(or protobuf-mode-map
    (setq protobuf-mode-map (c-make-inherited-keymap)))

(easy-menu-define protobuf-menu protobuf-mode-map
  "Protocol Buffers Mode Commands"
  (cons "Protocol Buffers" (c-lang-const c-mode-menu protobuf)))

;;;###autoload (add-to-list 'auto-mode-alist '("\\.proto\\'" . protobuf-mode))

;;;###autoload
(defun protobuf-mode ()
  "Major mode for editing Protocol Buffers description language.

The hook `c-mode-common-hook' is run with no argument at mode
initialization, then `protobuf-mode-hook'.

Key bindings:
\\{protobuf-mode-map}"
  (interactive)
  (kill-all-local-variables)
  (set-syntax-table protobuf-mode-syntax-table)
  (setq major-mode 'protobuf-mode
        mode-name "Protocol-Buffers"
        local-abbrev-table protobuf-mode-abbrev-table
        abbrev-mode t)
  (use-local-map protobuf-mode-map)
  (c-initialize-cc-mode t)
  (if (fboundp 'c-make-emacs-variables-local)
      (c-make-emacs-variables-local))
  (c-init-language-vars protobuf-mode)
  (c-common-init 'protobuf-mode)
  (easy-menu-add protobuf-menu)
  (c-run-mode-hooks 'c-mode-common-hook 'protobuf-mode-hook)
  (c-update-modeline))

(provide 'protobuf-mode)

;;; protobuf-mode.el ends here
