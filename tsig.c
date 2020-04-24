// tsig.c ... functions on Tuple Signatures (tsig's)
// part of SIMC signature files
// Written by John Shepherd, March 2020

#include <unistd.h>
#include <string.h>
#include "defs.h"
#include "tsig.h"
#include "reln.h"
#include "hash.h"
#include "bits.h"
#include "hash.h"
#include <stdlib.h>
#include "util.h"

//make a codeword for  tuple attrs
Bits tuple_codeword(char *attr, int m, int k){
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

// make a tuple signature
Bits makeTupleSig(Reln r, Tuple t)
{
	assert(r != NULL && t != NULL);
	//TODO
	//codeword
	Count m = tsigBits(r);
	Count k = codeBits(r);

	Count num_of_attrs = nAttrs(r);
	char ** attrVal = tupleVals(r, t);
	Bits res = newBits(m);
	//make code for each attr
	for (Count i = 0; i < num_of_attrs; i++){
	    if (strncmp(attrVal[i], "?", 1) != 0){
            Bits cw = tuple_codeword(attrVal[i], m, k);
            orBits(res, cw);
            freeBits(cw);
	    }
	}
	return res;
}

// find "matching" pages using tuple signatures

void findPagesUsingTupSigs(Query q)
{
	assert(q != NULL);
	//TODO

    unsetAllBits(q->pages);
    Reln r = q->rel;
    File tupleSigFile = tsigFile(r);
    Count m = tsigBits(r);
    //num of sig pages
    Count num_of_sig_page = nTsigPages(r);
    //max tuple sig per page
    Count max_tuple_sig_perpage = maxTsigsPP(r);
    //max tuple per page
    Count max_tuple_perpage = maxTupsPP(r);
    //make query sig
    Bits query_sig = makeTupleSig(r, q->qstring);

    for(PageID pd = 0; pd < num_of_sig_page; pd++){
        Page p = getPage(tupleSigFile, pd);
        for (Count tupleId = 0; tupleId < pageNitems(p); tupleId++){
            Bits tuple_sig = newBits(m);
            //get sig from page
            getBits(p, tupleId, tuple_sig);
            //match sig
            if (isSubset(query_sig, tuple_sig) == TRUE){
                Offset tuple_pos = pd * max_tuple_sig_perpage +tupleId;
                Offset page_pos = tuple_pos / max_tuple_perpage;
                setBit(q->pages, page_pos);
            }
            freeBits(tuple_sig);
            q->nsigs++;
        }
        q->nsigpages++;
    }


	// The printf below is primarily for debugging
	// Remove it before submitting this function
	//printf("Matched Pages:"); showBits(q->pages); putchar('\n');
}
