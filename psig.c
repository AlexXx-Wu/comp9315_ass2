// psig.c ... functions on page signatures (psig's)
// part of SIMC signature files
// Written by John Shepherd, March 2020

#include "defs.h"
#include "reln.h"
#include "query.h"
#include "psig.h"
#include "hash.h"
#include <stdlib.h>

// make  codeword
Bits page_codeword(char *attr, int m, int k){
    Count nbits = 0;
    Bits res = newBits(m);
    srandom(hash_any(attr, strlen(attr)));
    while (nbits < k){
        int i = random() % m;
        if (!bitIsSet(res, i)){
            setBit(res, i);
            nbits++;
        }
    }
    return res;
}

Bits makePageSig(Reln r, Tuple t)
{
	assert(r != NULL && t != NULL);
	//TODO
    Count m = psigBits(r);
    Count k = codeBits(r);

    Count num_of_attrs = nAttrs(r);
    char ** attrVal = tupleVals(r, t);
    Bits res = newBits(m);
    for (Count i = 0; i < num_of_attrs; i++){
        if (strncmp(attrVal[i], "?", 1) != 0){
            Bits cw = page_codeword(attrVal[i], m, k);
            orBits(res, cw);
            freeBits(cw);
        }
    }
    return res;
}

void findPagesUsingPageSigs(Query q)
{
	assert(q != NULL);
	//TODO
    unsetAllBits(q->pages);
    Reln r = q->rel;
    File pageSigFile = psigFile(r);
    Count m = psigBits(r);
    //num of sig pages
    Count num_of_sig_page = nPsigPages(r);
    //max tuple sig per page
    Count max_tuple_sig_perpage = maxPsigsPP(r);
    //max tuple per page
    //Count max_tuple_perpage = maxTupsPP(r);
    //make query sig
    Bits query_sig = makePageSig(r, q->qstring);

    for(PageID pd = 0; pd < num_of_sig_page; pd++){
        Page p = getPage(pageSigFile, pd);
        for (Count tupleId = 0; tupleId < pageNitems(p); tupleId++){
            Bits tuple_sig = newBits(m);
            //get sig from page
            getBits(p, tupleId, tuple_sig);
            //match sig
            if (isSubset(query_sig, tuple_sig) == TRUE){
                Offset  curr_pos = pd * max_tuple_sig_perpage + tupleId;
                setBit(q->pages, curr_pos);
            }
            freeBits(tuple_sig);
            q->nsigs++;
        }
        q->nsigpages++;
    }
}

