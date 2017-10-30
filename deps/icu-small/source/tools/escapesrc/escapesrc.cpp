// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <iostream>
#include <fstream>

// with caution:
#include "unicode/utf8.h"

static const char
  kSPACE   = 0x20,
  kTAB     = 0x09,
  kLF      = 0x0A,
  kCR      = 0x0D,
  // kHASH    = 0x23,
  // kSLASH   = 0x2f,
  kBKSLASH = 0x5C,
  // kSTAR    = 0x2A,
  kL_U     = 0x75,
  kU_U     = 0x55,
  kQUOT    = 0x27,
  kDBLQ    = 0x22;

# include "cptbl.h"

# define cp1047_to_8859(c) cp1047_8859_1[c]

std::string prog;

void usage() {
  fprintf(stderr, "%s: usage: %s infile.cpp outfile.cpp\n", prog.c_str(), prog.c_str());
}


int cleanup(const std::string &outfile) {
  const char *outstr = outfile.c_str();
  if(outstr && *outstr) {
    int rc = unlink(outstr);
    if(rc == 0) {
      fprintf(stderr, "%s: deleted %s\n", prog.c_str(), outstr);
      return 0;
    } else {
      if( errno == ENOENT ) {
        return 0; // File did not exist - no error.
      } else {
        perror("unlink");
        return 1;
      }
    }
  }
  return 0;
}

// inline bool hasNonAscii(const char *line, size_t len) {
//   const unsigned char *uline = reinterpret_cast<const unsigned char*>(line);
//   for(size_t i=0;i<len; i++) {
//     if( uline[i] > 0x7F) {
//       return true;
//     }
//   }
//   return false;
// }

inline const char *skipws(const char *p, const char *e) {
  for(;p<e;p++) {
    switch(*p) {
    case kSPACE:
    case kTAB:
    case kLF:
    case kCR:
      break;
    default:
      return p; // non ws
    }
  }
  return p;
}

// inline bool isCommentOrEmpty(const char* line, size_t len) {
//   const char *p = line;
//   const char *e = line+len;
//   p = skipws(p,e);
//   if(p==e) {
//     return true; // whitespace only
//   }
//   p++;
//   switch(*p) {
//   case kHASH: return true; // #directive
//   case kSLASH:
//     p++;
//     if(p==e) return false; // single slash
//     switch(*p) {
//     case kSLASH: // '/ /'
//     case kSTAR: // '/ *'
//       return true; // start of comment
//     default: return false; // something else
//     }
//   default: return false; // something else
//   }
//   /*NOTREACHED*/
// }

void appendByte(std::string &outstr,
                uint8_t byte) {
    char tmp2[5];
    sprintf(tmp2, "\\x%02X", 0xFF & (int)(byte));
    outstr += tmp2;
}

/**
 * @return true on failure
 */
bool appendUtf8(std::string &outstr,
                const std::string &linestr,
                size_t &pos,
                size_t chars) {
  char tmp[9];
  for(size_t i=0;i<chars;i++) {
    tmp[i] = linestr[++pos];
  }
  tmp[chars] = 0;
  unsigned int c;
  sscanf(tmp, "%X", &c);
  UChar32 ch = c & 0x1FFFFF;

  // now to append \\x%% etc
  uint8_t bytesNeeded = U8_LENGTH(ch);
  if(bytesNeeded == 0) {
    fprintf(stderr, "Illegal code point U+%X\n", ch);
    return true;
  }
  uint8_t bytes[4];
  uint8_t *s = bytes;
  size_t i = 0;
  U8_APPEND_UNSAFE(s, i, ch);
  for(size_t t = 0; t<i; t++) {
    appendByte(outstr, s[t]);
  }
  return false;
}

/**
 * @param linestr string to mutate. Already escaped into \u format.
 * @param origpos beginning, points to 'u8"'
 * @param pos end, points to "
 * @return false for no-problem, true for failure!
 */
bool fixu8(std::string &linestr, size_t origpos, size_t &endpos) {
  size_t pos = origpos + 3;
  std::string outstr;
  outstr += '\"'; // local encoding
  for(;pos<endpos;pos++) {
    char c = linestr[pos];
    if(c == '\\') {
      char c2 = linestr[++pos];
      switch(c2) {
      case '\'':
      case '"':
#if (U_CHARSET_FAMILY == U_EBCDIC_FAMILY)
        c2 = cp1047_to_8859(c2);
#endif
        appendByte(outstr, c2);
        break;
      case 'u':
        appendUtf8(outstr, linestr, pos, 4);
        break;
      case 'U':
        appendUtf8(outstr, linestr, pos, 8);
        break;
      }
    } else {
#if (U_CHARSET_FAMILY == U_EBCDIC_FAMILY)
      c = cp1047_to_8859(c);
#endif
      appendByte(outstr, c);
    }
  }
  outstr += ('\"');

  linestr.replace(origpos, (endpos-origpos+1), outstr);

  return false; // OK
}

/**
 * fix the string at the position
 * false = no err
 * true = had err
 */
bool fixAt(std::string &linestr, size_t pos) {
  size_t origpos = pos;

  if(linestr[pos] != 'u') {
    fprintf(stderr, "Not a 'u'?");
    return true;
  }

  pos++; // past 'u'

  bool utf8 = false;

  if(linestr[pos] == '8') { // u8"
    utf8 = true;
    pos++;
  }

  char quote = linestr[pos];

  if(quote != '\'' && quote != '\"') {
    fprintf(stderr, "Quote is '%c' - not sure what to do.\n", quote);
    return true;
  }

  if(quote == '\'' && utf8) {
    fprintf(stderr, "Cannot do u8'...'\n");
    return true;
  }

  pos ++;

  //printf("u%c…%c\n", quote, quote);

  for(; pos < linestr.size(); pos++) {
    if(linestr[pos] == quote) {
      if(utf8) {
        return fixu8(linestr, origpos, pos); // fix u8"..."
      } else {
        return false; // end of quote
      }
    }
    if(linestr[pos] == '\\') {
      pos++;
      if(linestr[pos] == quote) continue; // quoted quote
      if(linestr[pos] == 'u') continue; // for now ... unicode escape
      if(linestr[pos] == '\\') continue;
      // some other escape… ignore
    } else {
      size_t old_pos = pos;
      int32_t i = pos;
#if (U_CHARSET_FAMILY == U_EBCDIC_FAMILY)
      // mogrify 1-4 bytes from 1047 'back' to utf-8
      char old_byte = linestr[pos];
      linestr[pos] = cp1047_to_8859(linestr[pos]);
      // how many more?
      int32_t trail = U8_COUNT_TRAIL_BYTES(linestr[pos]);
      for(size_t pos2 = pos+1; trail>0; pos2++,trail--) {
        linestr[pos2] = cp1047_to_8859(linestr[pos2]);
        if(linestr[pos2] == 0x0A) {
          linestr[pos2] = 0x85; // NL is ambiguous here
        }
      }
#endif

      // Proceed to decode utf-8
      const uint8_t *s = (const uint8_t*) (linestr.c_str());
      int32_t length = linestr.size();
      UChar32 c;
      if(U8_IS_SINGLE((uint8_t)s[i]) && oldIllegal[s[i]]) {
#if (U_CHARSET_FAMILY == U_EBCDIC_FAMILY)
        linestr[pos] = old_byte; // put it back
#endif
        continue; // single code point not previously legal for \u escaping
      }

      // otherwise, convert it to \u / \U
      {
        U8_NEXT(s, i, length, c);
      }
      if(c<0) {
        fprintf(stderr, "Illegal utf-8 sequence at Column: %d\n", old_pos);
        fprintf(stderr, "Line: >>%s<<\n", linestr.c_str());
        return true;
      }

      size_t seqLen = (i-pos);

      //printf("U+%04X pos %d [len %d]\n", c, pos, seqLen);fflush(stdout);

      char newSeq[20];
      if( c <= 0xFFFF) {
        sprintf(newSeq, "\\u%04X", c);
      } else {
        sprintf(newSeq, "\\U%08X", c);
      }
      linestr.replace(pos, seqLen, newSeq);
      pos += strlen(newSeq) - 1;
    }
  }

  return false;
}

/**
 * false = no err
 * true = had err
 */
bool fixLine(int /*no*/, std::string &linestr) {
  const char *line = linestr.c_str();
  size_t len = linestr.size();

  // no u' in the line?
  if(!strstr(line, "u'") && !strstr(line, "u\"") && !strstr(line, "u8\"")) {
    return false; // Nothing to do. No u' or u" detected
  }

  // lines such as u8"\u0308" are all ASCII.
  // // Quick Check: all ascii?
  // if(!hasNonAscii(line, len)) {
  //   return false; // ASCII
  // }

  // // comment or empty line?
  // if(isCommentOrEmpty(line, len)) {
  //   return false; // Comment or just empty
  // }

  // start from the end and find all u" cases
  size_t pos = len = linestr.size();
  while((pos>0) && (pos = linestr.rfind("u\"", pos)) != std::string::npos) {
    //printf("found doublequote at %d\n", pos);
    if(fixAt(linestr, pos)) return true;
    if(pos == 0) break;
    pos--;
  }

  // reset and find all u' cases
  pos = len = linestr.size();
  while((pos>0) && (pos = linestr.rfind("u'", pos)) != std::string::npos) {
    //printf("found singlequote at %d\n", pos);
    if(fixAt(linestr, pos)) return true;
    if(pos == 0) break;
    pos--;
  }

  // reset and find all u8" cases
  pos = len = linestr.size();
  while((pos>0) && (pos = linestr.rfind("u8\"", pos)) != std::string::npos) {
    if(fixAt(linestr, pos)) return true;
    if(pos == 0) break;
    pos--;
  }

  //fprintf(stderr, "%d - fixed\n", no);
  return false;
}

int convert(const std::string &infile, const std::string &outfile) {
  fprintf(stderr, "escapesrc: %s -> %s\n", infile.c_str(), outfile.c_str());

  std::ifstream inf;

  inf.open(infile.c_str(), std::ios::in);

  if(!inf.is_open()) {
    fprintf(stderr, "%s: could not open input file %s\n", prog.c_str(), infile.c_str());
    cleanup(outfile);
    return 1;
  }

  std::ofstream outf;

  outf.open(outfile.c_str(), std::ios::out);

  if(!outf.is_open()) {
    fprintf(stderr, "%s: could not open output file %s\n", prog.c_str(), outfile.c_str());
    return 1;
  }

  // TODO: any platform variations of #line?
  outf << "#line 1 \"" << infile << "\"" << '\n';

  int no = 0;
  std::string linestr;
  while( getline( inf, linestr)) {
    no++;
    if(fixLine(no, linestr)) {
      outf.close();
      fprintf(stderr, "%s:%d: Fixup failed by %s\n", infile.c_str(), no, prog.c_str());
      cleanup(outfile);
      return 1;
    }
    outf << linestr << '\n';
  }

  return 0;
}

int main(int argc, const char *argv[]) {
  prog = argv[0];

  if(argc != 3) {
    usage();
    return 1;
  }

  std::string infile = argv[1];
  std::string outfile = argv[2];

  return convert(infile, outfile);
}


#include "utf_impl.cpp"
