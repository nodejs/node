/*
 *
 * IP-address/hostname to country converter.
 *
 * Problem; you want to know where IP a.b.c.d is located.
 *
 * Use ares_gethostbyname ("d.c.b.a.zz.countries.nerd.dk")
 * and get the CNAME (host->h_name). Result will be:
 *   CNAME = zz<CC>.countries.nerd.dk with address 127.0.x.y (ver 1) or
 *   CNAME = <a.b.c.d>.zz.countries.nerd.dk with address 127.0.x.y (ver 2)
 *
 * The 2 letter country code is in <CC> and the ISO-3166 country
 * number is in x.y (number = x*256 + y). Version 2 of the protocol is missing
 * the <CC> number.
 *
 * Ref: http://countries.nerd.dk/more.html
 *
 * Written by G. Vanem <gvanem@yahoo.no> 2006, 2007
 *
 * NB! This program may not be big-endian aware.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */

#include "ares_setup.h"

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#if defined(WIN32) && !defined(WATT32)
  #include <winsock.h>
#else
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <netdb.h>
#endif

#include "ares.h"
#include "ares_getopt.h"
#include "ares_nowarn.h"

#ifndef HAVE_STRDUP
#  include "ares_strdup.h"
#  define strdup(ptr) ares_strdup(ptr)
#endif

#ifndef HAVE_STRCASECMP
#  include "ares_strcasecmp.h"
#  define strcasecmp(p1,p2) ares_strcasecmp(p1,p2)
#endif

#ifndef HAVE_STRNCASECMP
#  include "ares_strcasecmp.h"
#  define strncasecmp(p1,p2,n) ares_strncasecmp(p1,p2,n)
#endif

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

/* By using a double cast, we can get rid of the bogus warning of
 * warning: cast from 'const struct sockaddr *' to 'const struct sockaddr_in6 *' increases required alignment from 1 to 4 [-Wcast-align]
 */
#define CARES_INADDR_CAST(type, var) ((type)((void *)var))

static const char *usage      = "acountry [-?hdv] {host|addr} ...\n";
static const char  nerd_fmt[] = "%u.%u.%u.%u.zz.countries.nerd.dk";
static const char *nerd_ver1  = nerd_fmt + 14;  /* .countries.nerd.dk */
static const char *nerd_ver2  = nerd_fmt + 11;  /* .zz.countries.nerd.dk */
static int         verbose    = 0;

#define TRACE(fmt) do {               \
                     if (verbose > 0) \
                       printf fmt ;   \
                   } WHILE_FALSE

static void wait_ares(ares_channel channel);
static void callback(void *arg, int status, int timeouts, struct hostent *host);
static void callback2(void *arg, int status, int timeouts, struct hostent *host);
static void find_country_from_cname(const char *cname, struct in_addr addr);
static void print_help_info_acountry(void);

static void Abort(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  exit(1);
}

int main(int argc, char **argv)
{
  ares_channel channel;
  int    ch, status;

#if defined(WIN32) && !defined(WATT32)
  WORD wVersionRequested = MAKEWORD(USE_WINSOCK,USE_WINSOCK);
  WSADATA wsaData;
  WSAStartup(wVersionRequested, &wsaData);
#endif

  status = ares_library_init(ARES_LIB_INIT_ALL);
  if (status != ARES_SUCCESS)
    {
      fprintf(stderr, "ares_library_init: %s\n", ares_strerror(status));
      return 1;
    }

  while ((ch = ares_getopt(argc, argv, "dvh?")) != -1)
    switch (ch)
      {
      case 'd':
#ifdef WATT32
        dbug_init();
#endif
        break;
      case 'v':
        verbose++;
        break;
      case 'h':
        print_help_info_acountry();
        break;
      case '?':
        print_help_info_acountry();
        break;
      default:
        Abort(usage);
      }

  argc -= optind;
  argv += optind;
  if (argc < 1)
     Abort(usage);

  status = ares_init(&channel);
  if (status != ARES_SUCCESS)
    {
      fprintf(stderr, "ares_init: %s\n", ares_strerror(status));
      return 1;
    }

  /* Initiate the queries, one per command-line argument. */
  for ( ; *argv; argv++)
    {
      struct in_addr addr;
      char *buf;

      /* If this fails, assume '*argv' is a host-name that
       * must be resolved first
       */
      if (ares_inet_pton(AF_INET, *argv, &addr) != 1)
        {
          ares_gethostbyname(channel, *argv, AF_INET, callback2, &addr);
          wait_ares(channel);
          if (addr.s_addr == INADDR_NONE)
            {
              printf("Failed to lookup %s\n", *argv);
              continue;
            }
        }

      buf = malloc(100);
      sprintf(buf, nerd_fmt,
              (unsigned int)(addr.s_addr >> 24),
              (unsigned int)((addr.s_addr >> 16) & 255),
              (unsigned int)((addr.s_addr >> 8) & 255),
              (unsigned int)(addr.s_addr & 255));
      TRACE(("Looking up %s...", buf));
      fflush(stdout);
      ares_gethostbyname(channel, buf, AF_INET, callback, buf);
    }

  wait_ares(channel);
  ares_destroy(channel);

  ares_library_cleanup();

#if defined(WIN32) && !defined(WATT32)
  WSACleanup();
#endif

  return 0;
}

/*
 * Wait for the queries to complete.
 */
static void wait_ares(ares_channel channel)
{
  for (;;)
    {
      struct timeval *tvp, tv;
      fd_set read_fds, write_fds;
      int nfds;

      FD_ZERO(&read_fds);
      FD_ZERO(&write_fds);
      nfds = ares_fds(channel, &read_fds, &write_fds);
      if (nfds == 0)
        break;
      tvp = ares_timeout(channel, NULL, &tv);
      nfds = select(nfds, &read_fds, &write_fds, NULL, tvp);
      if (nfds < 0)
        continue;
      ares_process(channel, &read_fds, &write_fds);
    }
}

/*
 * This is the callback used when we have the IP-address of interest.
 * Extract the CNAME and figure out the country-code from it.
 */
static void callback(void *arg, int status, int timeouts, struct hostent *host)
{
  const char *name = (const char*)arg;
  const char *cname;
  char buf[20];

  (void)timeouts;

  if (!host || status != ARES_SUCCESS)
    {
      printf("Failed to lookup %s: %s\n", name, ares_strerror(status));
      free(arg);
      return;
    }

  TRACE(("\nFound address %s, name %s\n",
         ares_inet_ntop(AF_INET,(const char*)host->h_addr,buf,sizeof(buf)),
         host->h_name));

  cname = host->h_name;  /* CNAME gets put here */
  if (!cname)
    printf("Failed to get CNAME for %s\n", name);
  else
    find_country_from_cname(cname, *(CARES_INADDR_CAST(struct in_addr *, host->h_addr)));
  free(arg);
}

/*
 * This is the callback used to obtain the IP-address of the host of interest.
 */
static void callback2(void *arg, int status, int timeouts, struct hostent *host)
{
  struct in_addr *addr = (struct in_addr*) arg;

  (void)timeouts;
  if (!host || status != ARES_SUCCESS)
    memset(addr, INADDR_NONE, sizeof(*addr));
  else
    memcpy(addr, host->h_addr, sizeof(*addr));
}

struct search_list {
       int         country_number; /* ISO-3166 country number */
       char        short_name[3];  /* A2 short country code */
       const char *long_name;      /* normal country name */
     };

static const struct search_list *list_lookup(int number, const struct search_list *list, int num)
{
  while (num > 0 && list->long_name)
    {
      if (list->country_number == number)
        return (list);
      num--;
      list++;
    }
  return (NULL);
}

/*
 * Ref: https://en.wikipedia.org/wiki/ISO_3166-1
 */
static const struct search_list country_list[] = {
       {   4, "af", "Afghanistan"                          },
       { 248, "ax", "Åland Island"                         },
       {   8, "al", "Albania"                              },
       {  12, "dz", "Algeria"                              },
       {  16, "as", "American Samoa"                       },
       {  20, "ad", "Andorra"                              },
       {  24, "ao", "Angola"                               },
       { 660, "ai", "Anguilla"                             },
       {  10, "aq", "Antarctica"                           },
       {  28, "ag", "Antigua & Barbuda"                    },
       {  32, "ar", "Argentina"                            },
       {  51, "am", "Armenia"                              },
       { 533, "aw", "Aruba"                                },
       {  36, "au", "Australia"                            },
       {  40, "at", "Austria"                              },
       {  31, "az", "Azerbaijan"                           },
       {  44, "bs", "Bahamas"                              },
       {  48, "bh", "Bahrain"                              },
       {  50, "bd", "Bangladesh"                           },
       {  52, "bb", "Barbados"                             },
       { 112, "by", "Belarus"                              },
       {  56, "be", "Belgium"                              },
       {  84, "bz", "Belize"                               },
       { 204, "bj", "Benin"                                },
       {  60, "bm", "Bermuda"                              },
       {  64, "bt", "Bhutan"                               },
       {  68, "bo", "Bolivia"                              },
       { 535, "bq", "Bonaire, Sint Eustatius and Saba"     }, /* Formerly 'Bonaire' / 'Netherlands Antilles' */
       {  70, "ba", "Bosnia & Herzegovina"                 },
       {  72, "bw", "Botswana"                             },
       {  74, "bv", "Bouvet Island"                        },
       {  76, "br", "Brazil"                               },
       {  86, "io", "British Indian Ocean Territory"       },
       {  96, "bn", "Brunei Darussalam"                    },
       { 100, "bg", "Bulgaria"                             },
       { 854, "bf", "Burkina Faso"                         },
       { 108, "bi", "Burundi"                              },
       { 116, "kh", "Cambodia"                             },
       { 120, "cm", "Cameroon"                             },
       { 124, "ca", "Canada"                               },
       { 132, "cv", "Cape Verde"                           },
       { 136, "ky", "Cayman Islands"                       },
       { 140, "cf", "Central African Republic"             },
       { 148, "td", "Chad"                                 },
       { 152, "cl", "Chile"                                },
       { 156, "cn", "China"                                },
       { 162, "cx", "Christmas Island"                     },
       { 166, "cc", "Cocos Islands"                        },
       { 170, "co", "Colombia"                             },
       { 174, "km", "Comoros"                              },
       { 178, "cg", "Congo"                                },
       { 180, "cd", "Congo"                                },
       { 184, "ck", "Cook Islands"                         },
       { 188, "cr", "Costa Rica"                           },
       { 384, "ci", "Cote d'Ivoire"                        },
       { 191, "hr", "Croatia"                              },
       { 192, "cu", "Cuba"                                 },
       { 531, "cw", "Curaçao"                              },
       { 196, "cy", "Cyprus"                               },
       { 203, "cz", "Czech Republic"                       },
       { 208, "dk", "Denmark"                              },
       { 262, "dj", "Djibouti"                             },
       { 212, "dm", "Dominica"                             },
       { 214, "do", "Dominican Republic"                   },
       { 218, "ec", "Ecuador"                              },
       { 818, "eg", "Egypt"                                },
       { 222, "sv", "El Salvador"                          },
       { 226, "gq", "Equatorial Guinea"                    },
       { 232, "er", "Eritrea"                              },
       { 233, "ee", "Estonia"                              },
       { 748, "sz", "Eswatini"                             }, /* Formerly Swaziland */
       { 231, "et", "Ethiopia"                             },
     { 65281, "eu", "European Union"                       }, /* 127.0.255.1 */
       { 238, "fk", "Falkland Islands"                     },
       { 234, "fo", "Faroe Islands"                        },
       { 242, "fj", "Fiji"                                 },
       { 246, "fi", "Finland"                              },
       { 250, "fr", "France"                               },
       { 249, "fx", "France, Metropolitan"                 },
       { 254, "gf", "French Guiana"                        },
       { 258, "pf", "French Polynesia"                     },
       { 260, "tf", "French Southern Territories"          },
       { 266, "ga", "Gabon"                                },
       { 270, "gm", "Gambia"                               },
       { 268, "ge", "Georgia"                              },
       { 276, "de", "Germany"                              },
       { 288, "gh", "Ghana"                                },
       { 292, "gi", "Gibraltar"                            },
       { 300, "gr", "Greece"                               },
       { 304, "gl", "Greenland"                            },
       { 308, "gd", "Grenada"                              },
       { 312, "gp", "Guadeloupe"                           },
       { 316, "gu", "Guam"                                 },
       { 320, "gt", "Guatemala"                            },
       { 831, "gg", "Guernsey"                             },
       { 324, "gn", "Guinea"                               },
       { 624, "gw", "Guinea-Bissau"                        },
       { 328, "gy", "Guyana"                               },
       { 332, "ht", "Haiti"                                },
       { 334, "hm", "Heard & Mc Donald Islands"            },
       { 336, "va", "Holy See"                             }, /* Vatican City */
       { 340, "hn", "Honduras"                             },
       { 344, "hk", "Hong kong"                            },
       { 348, "hu", "Hungary"                              },
       { 352, "is", "Iceland"                              },
       { 356, "in", "India"                                },
       { 360, "id", "Indonesia"                            },
       { 364, "ir", "Iran"                                 },
       { 368, "iq", "Iraq"                                 },
       { 372, "ie", "Ireland"                              },
       { 833, "im", "Isle of Man"                          },
       { 376, "il", "Israel"                               },
       { 380, "it", "Italy"                                },
       { 388, "jm", "Jamaica"                              },
       { 392, "jp", "Japan"                                },
       { 832, "je", "Jersey"                               },
       { 400, "jo", "Jordan"                               },
       { 398, "kz", "Kazakhstan"                           },
       { 404, "ke", "Kenya"                                },
       { 296, "ki", "Kiribati"                             },
       { 408, "kp", "Korea (north)"                        },
       { 410, "kr", "Korea (south)"                        },
       {   0, "xk", "Kosovo"                               }, /* https://en.wikipedia.org/wiki/ISO_3166-1_alpha-2 */
       { 414, "kw", "Kuwait"                               },
       { 417, "kg", "Kyrgyzstan"                           },
       { 418, "la", "Laos"                                 },
       { 428, "lv", "Latvia"                               },
       { 422, "lb", "Lebanon"                              },
       { 426, "ls", "Lesotho"                              },
       { 430, "lr", "Liberia"                              },
       { 434, "ly", "Libya"                                },
       { 438, "li", "Liechtenstein"                        },
       { 440, "lt", "Lithuania"                            },
       { 442, "lu", "Luxembourg"                           },
       { 446, "mo", "Macao"                                },
       { 450, "mg", "Madagascar"                           },
       { 454, "mw", "Malawi"                               },
       { 458, "my", "Malaysia"                             },
       { 462, "mv", "Maldives"                             },
       { 466, "ml", "Mali"                                 },
       { 470, "mt", "Malta"                                },
       { 584, "mh", "Marshall Islands"                     },
       { 474, "mq", "Martinique"                           },
       { 478, "mr", "Mauritania"                           },
       { 480, "mu", "Mauritius"                            },
       { 175, "yt", "Mayotte"                              },
       { 484, "mx", "Mexico"                               },
       { 583, "fm", "Micronesia"                           },
       { 498, "md", "Moldova"                              },
       { 492, "mc", "Monaco"                               },
       { 496, "mn", "Mongolia"                             },
       { 499, "me", "Montenegro"                           },  
       { 500, "ms", "Montserrat"                           },
       { 504, "ma", "Morocco"                              },
       { 508, "mz", "Mozambique"                           },
       { 104, "mm", "Myanmar"                              },
       { 516, "na", "Namibia"                              },
       { 520, "nr", "Nauru"                                },
       { 524, "np", "Nepal"                                },
       { 528, "nl", "Netherlands"                          },
       { 540, "nc", "New Caledonia"                        },
       { 554, "nz", "New Zealand"                          },
       { 558, "ni", "Nicaragua"                            },
       { 562, "ne", "Niger"                                },
       { 566, "ng", "Nigeria"                              },
       { 570, "nu", "Niue"                                 },
       { 574, "nf", "Norfolk Island"                       },
       { 807, "mk", "North Macedonia"                      }, /* 'Macedonia' until February 2019 */
       { 580, "mp", "Northern Mariana Islands"             },
       { 578, "no", "Norway"                               },
       { 512, "om", "Oman"                                 },
       { 586, "pk", "Pakistan"                             },
       { 585, "pw", "Palau"                                },
       { 275, "ps", "Palestinian Territory"                },
       { 591, "pa", "Panama"                               },
       { 598, "pg", "Papua New Guinea"                     },
       { 600, "py", "Paraguay"                             },
       { 604, "pe", "Peru"                                 },
       { 608, "ph", "Philippines"                          },
       { 612, "pn", "Pitcairn"                             },
       { 616, "pl", "Poland"                               },
       { 620, "pt", "Portugal"                             },
       { 630, "pr", "Puerto Rico"                          },
       { 634, "qa", "Qatar"                                },
       { 638, "re", "Reunion"                              },
       { 642, "ro", "Romania"                              },
       { 643, "ru", "Russian Federation"                   },
       { 646, "rw", "Rwanda"                               },
       { 0,   "bl", "Saint Barthélemy"                     }, /* https://en.wikipedia.org/wiki/ISO_3166-2:BL */
       { 659, "kn", "Saint Kitts & Nevis"                  },
       { 662, "lc", "Saint Lucia"                          },
       { 663, "mf", "Saint Martin"                         },
       { 670, "vc", "Saint Vincent"                        },
       { 882, "ws", "Samoa"                                },
       { 674, "sm", "San Marino"                           },
       { 678, "st", "Sao Tome & Principe"                  },
       { 682, "sa", "Saudi Arabia"                         },
       { 686, "sn", "Senegal"                              },
       { 688, "rs", "Serbia"                               },
       { 690, "sc", "Seychelles"                           },
       { 694, "sl", "Sierra Leone"                         },
       { 702, "sg", "Singapore"                            },
       { 534, "sx", "Sint Maarten"                         },
       { 703, "sk", "Slovakia"                             },
       { 705, "si", "Slovenia"                             },
       {  90, "sb", "Solomon Islands"                      },
       { 706, "so", "Somalia"                              },
       { 710, "za", "South Africa"                         },
       { 239, "gs", "South Georgia & South Sandwich Is."   },
       { 728, "ss", "South Sudan"                          },
       { 724, "es", "Spain"                                },
       { 144, "lk", "Sri Lanka"                            },
       { 654, "sh", "St. Helena"                           },
       { 666, "pm", "St. Pierre & Miquelon"                },
       { 736, "sd", "Sudan"                                },
       { 740, "sr", "Suriname"                             },
       { 744, "sj", "Svalbard & Jan Mayen Islands"         },
       { 752, "se", "Sweden"                               },
       { 756, "ch", "Switzerland"                          },
       { 760, "sy", "Syrian Arab Republic"                 },
       { 158, "tw", "Taiwan"                               },
       { 762, "tj", "Tajikistan"                           },
       { 834, "tz", "Tanzania"                             },
       { 764, "th", "Thailand"                             },
       { 626, "tl", "Timor-Leste"                          },
       { 768, "tg", "Togo"                                 },
       { 772, "tk", "Tokelau"                              },
       { 776, "to", "Tonga"                                },
       { 780, "tt", "Trinidad & Tobago"                    },
       { 788, "tn", "Tunisia"                              },
       { 792, "tr", "Turkey"                               },
       { 795, "tm", "Turkmenistan"                         },
       { 796, "tc", "Turks & Caicos Islands"               },
       { 798, "tv", "Tuvalu"                               },
       { 800, "ug", "Uganda"                               },
       { 804, "ua", "Ukraine"                              },
       { 784, "ae", "United Arab Emirates"                 },
       { 826, "gb", "United Kingdom"                       },
       { 840, "us", "United States"                        },
       { 581, "um", "United States Minor Outlying Islands" },
       { 858, "uy", "Uruguay"                              },
       { 860, "uz", "Uzbekistan"                           },
       { 548, "vu", "Vanuatu"                              },
       { 862, "ve", "Venezuela"                            },
       { 704, "vn", "Vietnam"                              },
       {  92, "vg", "Virgin Islands (British)"             },
       { 850, "vi", "Virgin Islands (US)"                  },
       { 876, "wf", "Wallis & Futuna Islands"              },
       { 732, "eh", "Western Sahara"                       },
       { 887, "ye", "Yemen"                                },
       { 894, "zm", "Zambia"                               },
       { 716, "zw", "Zimbabwe"                             }
     };

/*
 * Check if start of 'str' is simply an IPv4 address.
 */
#define BYTE_OK(x) ((x) >= 0 && (x) <= 255)

static int is_addr(char *str, char **end)
{
  int a0, a1, a2, a3, num, rc = 0, length = 0;

  num = sscanf(str,"%3d.%3d.%3d.%3d%n",&a0,&a1,&a2,&a3,&length);
  if( (num == 4) &&
      BYTE_OK(a0) && BYTE_OK(a1) && BYTE_OK(a2) && BYTE_OK(a3) &&
      length >= (3+4))
    {
      rc = 1;
      *end = str + length;
    }
  return rc;
}

/*
 * Find the country-code and name from the CNAME. E.g.:
 *   version 1: CNAME = zzno.countries.nerd.dk with address 127.0.2.66
 *              yields ccode_A" = "no" and cnumber 578 (2.66).
 *   version 2: CNAME = <a.b.c.d>.zz.countries.nerd.dk with address 127.0.2.66
 *              yields cnumber 578 (2.66). ccode_A is "";
 */
static void find_country_from_cname(const char *cname, struct in_addr addr)
{
  const struct search_list *country;
  char  ccode_A2[3], *ccopy, *dot_4;
  int   cnumber, z0, z1, ver_1, ver_2;
  unsigned long ip;

  ip = ntohl(addr.s_addr);
  z0 = TOLOWER(cname[0]);
  z1 = TOLOWER(cname[1]);
  ccopy = strdup(cname);
  dot_4 = NULL;

  ver_1 = (z0 == 'z' && z1 == 'z' && !strcasecmp(cname+4,nerd_ver1));
  ver_2 = (is_addr(ccopy,&dot_4) && !strcasecmp(dot_4,nerd_ver2));

  if (ver_1)
    {
      const char *dot = strchr(cname, '.');
      if (dot != cname+4)
        {
          printf("Unexpected CNAME %s (ver_1)\n", cname);
          free(ccopy);
          return;
        }
    }
  else if (ver_2)
    {
      z0 = TOLOWER(dot_4[1]);
      z1 = TOLOWER(dot_4[2]);
      if (z0 != 'z' && z1 != 'z')
        {
          printf("Unexpected CNAME %s (ver_2)\n", cname);
          free(ccopy);
          return;
        }
    }
  else
    {
      printf("Unexpected CNAME %s (ver?)\n", cname);
      free(ccopy);
      return;
    }

  if (ver_1)
    {
      ccode_A2[0] = (char)TOLOWER(cname[2]);
      ccode_A2[1] = (char)TOLOWER(cname[3]);
      ccode_A2[2] = '\0';
    }
  else
    ccode_A2[0] = '\0';

  cnumber = ip & 0xFFFF;

  TRACE(("Found country-code `%s', number %d\n",
         ver_1 ? ccode_A2 : "<n/a>", cnumber));

  country = list_lookup(cnumber, country_list,
                        sizeof(country_list) / sizeof(country_list[0]));
  if (!country)
    printf("Name for country-number %d not found.\n", cnumber);
  else
    {
      if (ver_1)
        {
          if ((country->short_name[0] != ccode_A2[0]) ||
              (country->short_name[1] != ccode_A2[1]) ||
              (country->short_name[2] != ccode_A2[2]))
            printf("short-name mismatch; %s vs %s\n",
                   country->short_name, ccode_A2);
        }
      printf("%s (%s), number %d.\n",
             country->long_name, country->short_name, cnumber);
    }
  free(ccopy);
}

/* Information from the man page. Formatting taken from man -h */
static void print_help_info_acountry(void) {
    printf("acountry, version %s\n\n", ARES_VERSION_STR);
    printf("usage: acountry [-hdv] host|addr ...\n\n"
    "  h : Display this help and exit.\n"
    "  d : Print some extra debugging output.\n"
    "  v : Be more verbose. Print extra information.\n\n");
    exit(0);
}
