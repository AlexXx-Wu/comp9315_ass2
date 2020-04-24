// bsig.c ... functions on Tuple Signatures (bsig's)
// part of SIMC signature files
// Written by John Shepherd, March 2020

#include "defs.h"
#include "reln.h"
#include "query.h"
#include "bsig.h"
#include "psig.c"

void findPagesUsingBitSlices(Query q)
{
	assert(q != NULL);
	//TODO
    Reln r = q->rel;
    // set max page
	setAllBits(q->pages);
    //max bit-slices per page
    Count max_bit_sliced_sig_pp = maxBsigsPP(r);
    //width of bit-slice (=maxpages)
    Count m_of_sliced_page = bsigBits(r);
    // width of page signature (#bits)
    Count m_of_bits_page_sig = psigBits(r);
	//open sliced file
	File bits_sliced_sig_file = bsigFile(r);
	Bits query_page = makePageSig(r, q->qstring);
	Bits new_query_page = newBits(m_of_sliced_page);
    setAllBits(new_query_page);
	PageID pd = -1;
	Page page = NULL;

	for (Count idx = 0; idx < m_of_bits_page_sig; idx++){
	    if (bitIsSet(query_page, idx) == TRUE){
	        if (pd != idx / max_bit_sliced_sig_pp){
                pd = idx / max_bit_sliced_sig_pp;
                page = getPage(bits_sliced_sig_file, pd);
                q->nsigpages++;
	        }

	        Offset position = idx % max_bit_sliced_sig_pp;

	        Bits bits = newBits(m_of_sliced_page);
	        getBits(page, position, bits);

	        for (Offset pos = 0; pos < m_of_sliced_page; pos++){
	            if (bitIsSet(bits, pos) == FALSE) {
                    unsetBit(new_query_page, pos);
                }
	        }
            freeBits(bits);
	        q->nsigs++;
	    }
	}
	q->pages = new_query_page;
}

