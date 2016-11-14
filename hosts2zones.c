//  hosts2zones.c
//  hosts2zones
//
//  Created by Dr. Rolf Jansen on 2014-07-06.
//  Copyright (c) 2014 projectworld.net. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without modification,
//  are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
//  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
//  AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
//  OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
//  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
//  THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

#include "binutils.h"
#include "store.h"

#define hcnt 65535

int     hosts_count = 0;
int     zones_count = 0;
FILE   *in, *out;
Node  **domainStore;

void zoneOutput(Node *node)
{
   if (node)
   {
      zoneOutput(node->L);
      zoneOutput(node->R);

      bool  ontop = true;
      int   n, level = 0;
      char *q, *p = node->name;

      for (q = p + node->naml, n = 0; ontop && q > p; q--, n++)
         if (*q && *(q-1) == '.' && (++level >= 3 || level == 2 && n > 6) && findName(domainStore, q, n))
            ontop = false;

      if (ontop && level && !node->value.b)
      {
         fprintf(out, "local-zone: \"%s\" static\n", node->name);
         zones_count++;
      }
   }
}


int main(int argc, const char *argv[])
{
   if (argc >= 3 && (out = fopen(argv[1], "w")))
   {
      domainStore = createTable(hcnt);

      int    inc;
      struct stat st;
      for (inc = 2; inc < argc; inc++)
         if (stat(argv[inc], &st) == noerr && st.st_size > 3 && (in = fopen(argv[inc], "r")))
         {
            char *hosts = allocate(st.st_size+2, false);
            if (fread(hosts, st.st_size, 1, in) == 1)
            {
               *(uint16_t *)&hosts[st.st_size] = *(uint16_t *)"\n";

               bool  iswhite;
               int   ll, wl;
               char *word, *lineend, *line = hosts + blanklen(hosts);
               char *nextword, *nextline;

               while (line < hosts + st.st_size)
               {
                  ll = linelen(line); *(lineend = bskip(line+ll)) = '\0';              // skip trailing blanks
                  nextline  = line + ll+1;                                             // get the start of the next line
                  nextline += blanklen(nextline);                                      // by skipping leading blanks and blank lines

                  if (*line != '#')                                                    // skip comment lines
                  {
                     word = NULL;                                                      // assume inacceptable entry

                     wl = wordlen(line); line[wl] = '\0';                              // pickup the first entry on the line
                     if (line+wl == lineend && *line != '.' && domainlen(line) < wl)   // accept a single domain name per line that must contain at least 1 non-leading/non-traling dot
                        iswhite = false, word = line;                                  // simple domain lists are always black lists

                     else if ((iswhite = *(int64_t *)line == *(int64_t *)"1.1.1.1") || // otherwise assume the Hosts file format
                             /*isblack*/ *(int64_t *)line == *(int64_t *)"0.0.0.0"  || // entrie starting with 1.1.1.1 shall be white listed
                             /*isblack*/ *(int16_t *)line == *(int16_t *)"12" &&       // 0.0.0.0 or 127.0.0.1 are black list entries
                                    *(int64_t *)(line+=2) == *(int64_t *)"7.0.0.1")
                        word = line + 8, word += blanklen(word);                       // skip address and leading blanks

                     if (word)                                                         // process only simple domain entries or entries in Hosts file format
                        while (word && *word && *word != '#' && word < nextline)       // Hosts files may contain more than one domain per line
                        {
                           wl = wordlen(word);
                           nextword = word + wl;
                           *nextword++ = '\0';
                           nextword += blanklen(nextword);

                           if (*lowercase(word, wl) &&                                 // convert to lowercase, end check if it is a non empty
                               (wl != 9 || *word != 'l' || *(int64_t *)(word+1) != *(int64_t *)"ocalhost") && !findName(domainStore, word, wl))
                           {
                              Value value = {Simple, .b = iswhite};
                              storeName(domainStore, word, wl, &value);
                              hosts_count++;
                           }

                           word = nextword;
                        }
                  }

                  line = nextline;
               }
            }

            deallocate(VPR(hosts), false);
            fclose(in);
         }

      for (size_t h = 1; h <= hcnt; h++)
         zoneOutput(domainStore[h]);

      releaseTable(domainStore);
      fclose(out);

      printf("Number of Hosts: %d\nNumber of Zones: %d\n", hosts_count, zones_count);

      return (zones_count > 0) ? 0 : 1;
   }

   return 1;
}
