// reln.c ... functions on Relations
// part of SIMC signature files
// Written by John Shepherd, March 2020

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "defs.h"
#include "reln.h"
#include "page.h"
#include "tuple.h"
#include "tsig.h"
#include "bits.h"
#include "hash.h"
#include "psig.h"
// open a file with a specified suffix
// - always open for both reading and writing

File openFile(char *name, char *suffix)
{
	char fname[MAXFILENAME];
	sprintf(fname,"%s.%s",name,suffix);
	File f = open(fname,O_RDWR|O_CREAT,0644);
	assert(f >= 0);
	return f;
}

// create a new relation (five files)
// data file has one empty data page

Status newRelation(char *name, Count nattrs, float pF,
                   Count tk, Count tm, Count pm, Count bm)
{
	Reln r = malloc(sizeof(RelnRep));
	RelnParams *p = &(r->params);
	assert(r != NULL);
	p->nattrs = nattrs;
	p->pF = pF,
	p->tupsize = 28 + 7*(nattrs-2);
	Count available = (PAGESIZE-sizeof(Count));
	p->tupPP = available/p->tupsize;
	p->tk = tk; 
	if (tm%8 > 0) tm += 8-(tm%8); // round up to byte size
	p->tm = tm; p->tsigSize = tm/8; p->tsigPP = available/(tm/8);
	if (pm%8 > 0) pm += 8-(pm%8); // round up to byte size
	p->pm = pm; p->psigSize = pm/8; p->psigPP = available/(pm/8);
	if (p->psigPP < 2) { free(r); return -1; }
	if (bm%8 > 0) bm += 8-(bm%8); // round up to byte size
	p->bm = bm; p->bsigSize = bm/8; p->bsigPP = available/(bm/8);
	if (p->bsigPP < 2) { free(r); return -1; }
	r->infof = openFile(name,"info");
	r->dataf = openFile(name,"data");
	r->tsigf = openFile(name,"tsig");
	r->psigf = openFile(name,"psig");
	r->bsigf = openFile(name,"bsig");
	addPage(r->dataf); p->npages = 1; p->ntups = 0;
	addPage(r->tsigf); p->tsigNpages = 1; p->ntsigs = 0;
	addPage(r->psigf); p->psigNpages = 1; p->npsigs = 0;
	addPage(r->bsigf); p->bsigNpages = 1; p->nbsigs = 0; // replace this
	// Create a file containing "pm" all-zeroes bit-strings,
    // each of which has length "bm" bits
	//TODO

	File bit_sliced_file = bsigFile(r);
	Count m_of_page_sig = psigBits(r);
	Count max_bit_sliced_pp = maxBsigsPP(r);
	Count m_of_bits_sig = bsigBits(r);

	Offset idx = 0;
	while (idx < m_of_page_sig){
	    PageID pd = p->bsigNpages - 1;
	    Page page = getPage(bit_sliced_file, pd);

	    if (pageNitems(page) ==max_bit_sliced_pp){
	        addPage(bit_sliced_file);
	        p->bsigNpages++;
	        pd++;

	        free(page);
            page = newPage();
            if (page == NULL) return NO_PAGE;
	    }
	    Bits bit_sig =newBits(m_of_bits_sig);
	    putBits(page, pageNitems(page), bit_sig);
	    addOneItem(page);
	    p->nbsigs++;

	    putPage(bit_sliced_file,pd, page);
	    free(bit_sig);

	    idx++;
	}
	closeRelation(r);
	return 0;
}

// check whether a relation already exists

Bool existsRelation(char *name)
{
	char fname[MAXFILENAME];
	sprintf(fname,"%s.info",name);
	File f = open(fname,O_RDONLY);
	if (f < 0)
		return FALSE;
	else {
		close(f);
		return TRUE;
	}
}

// set up a relation descriptor from relation name
// open files, reads information from rel.info

Reln openRelation(char *name)
{
	Reln r = malloc(sizeof(RelnRep));
	assert(r != NULL);
	r->infof = openFile(name,"info");
	r->dataf = openFile(name,"data");
	r->tsigf = openFile(name,"tsig");
	r->psigf = openFile(name,"psig");
	r->bsigf = openFile(name,"bsig");
	read(r->infof, &(r->params), sizeof(RelnParams));
	return r;
}

// release files and descriptor for an open relation
// copy latest information to .info file
// note: we don't write ChoiceVector since it doesn't change

void closeRelation(Reln r)
{
	// make sure updated global data is put in info file
	lseek(r->infof, 0, SEEK_SET);
	int n = write(r->infof, &(r->params), sizeof(RelnParams));
	assert(n == sizeof(RelnParams));
	close(r->infof); close(r->dataf);
	close(r->tsigf); close(r->psigf); close(r->bsigf);
	free(r);
}

// insert a new tuple into a relation
// returns page where inserted
// returns NO_PAGE if insert fails completely

PageID addToRelation(Reln r, Tuple t)
{
	assert(r != NULL && t != NULL && strlen(t) == tupSize(r));
	Page p;  PageID pid;
	RelnParams *rp = &(r->params);
	
	// add tuple to last page
	pid = rp->npages-1;
	p = getPage(r->dataf, pid);
	// check if room on last page; if not add new page
	Bool check = FALSE;
	if (pageNitems(p) == rp->tupPP) {
		addPage(r->dataf);
		rp->npages++;
		pid++;
		free(p);
		p = newPage();
		check = TRUE;
		if (p == NULL) return NO_PAGE;
	}
	addTupleToPage(r, p, t);
	rp->ntups++;  //written to disk in closeRelation()
	putPage(r->dataf, pid, p);

	// compute tuple signature and add to tsigf
	
	//TODO
	Bits tuple_sig = makeTupleSig(r, t);
	File tuple_sig_file = tsigFile(r);
	Count num_of_tuple_sig_page = nTsigPages(r);
	PageID pd = num_of_tuple_sig_page - 1;
	Page curr_tuple_sig_page = getPage(tuple_sig_file, pd);
    // if full

    if (pageNitems(curr_tuple_sig_page) == maxTsigsPP(r)) {
        addPage(tuple_sig_file);
        rp->tsigNpages++;
        pd++;
        free(curr_tuple_sig_page);
        curr_tuple_sig_page = newPage();
        if (curr_tuple_sig_page == NULL) return NO_PAGE;
    }
    putBits(curr_tuple_sig_page, pageNitems(curr_tuple_sig_page), tuple_sig);
    addOneItem(curr_tuple_sig_page);
    putPage(tuple_sig_file, pd, curr_tuple_sig_page);
    rp->ntsigs++;
    freeBits(tuple_sig);

	// compute page signature and add to psigf

	//TODO
	Bits page_sig = makePageSig(r, t);
	File page_sig_file = psigFile(r);
	Count num_of_page_sig_page = nPsigPages(r);
	pd = num_of_page_sig_page - 1;
	Page page = getPage(page_sig_file, pd);
	Count m = psigBits(r);
	Count max_tuple_of_page_sig = maxPsigsPP(r);

    if (pageNitems(page) == 0){
        putBits(page, pageNitems(page),page_sig);
        addOneItem(page);
        putPage(page_sig_file,pd,page);
        rp->npsigs++;
    }else{
        if (check == TRUE){
            if (pageNitems(page) == max_tuple_of_page_sig){
                addPage(page_sig_file);
                rp->psigNpages++;
                pd++;
                free(page);
                page = newPage();
                if (page == NULL) return NO_PAGE;
            }
            putBits(page, pageNitems(page),page_sig);
            addOneItem(page);
            putPage(page_sig_file,pd,page);
            rp->npsigs++;
        }else{
            //num_of_tuple_sig_page or num_of_page_sig_page
            Offset pos = num_of_tuple_sig_page - 1;
            Bits curr_page_sign = newBits(m);
            getBits(page, pos, curr_page_sign);
            orBits(curr_page_sign, page_sig);
            putBits(page,pos, curr_page_sign);
            putPage(page_sig_file,pd,page);
            freeBits(curr_page_sign);

        }
    }
	// use page signature to update bit-slices

	//TODO
	File bit_sliced_file = bsigFile(r);
    Count max_bit_sliced_sig_pp = maxBsigsPP(r);
    Count m_of_sliced_page = bsigBits(r);

    for (Offset idx = 0; idx < m; idx++){
        if (bitIsSet(page_sig, idx) == TRUE){
            if (pd != idx / max_bit_sliced_sig_pp){
                pd = idx / max_bit_sliced_sig_pp;
                page = getPage(bit_sliced_file, pd);
            }

            Offset position = idx % max_bit_sliced_sig_pp;
            Bits bits = newBits(m_of_sliced_page);
            getBits(page, position, bits);
            setBit(bits, pid);
            putBits(page, position, bits);
            putPage(bit_sliced_file, pd, page);
            freeBits(bits);
        }
    }
    freeBits(page_sig);
	return nPages(r)-1;
}

// displays info about open Reln (for debugging)

void relationStats(Reln r)
{
	RelnParams *p = &(r->params);
	printf("Global Info:\n");
	printf("Dynamic:\n");
    printf("  #items:  tuples: %d  tsigs: %d  psigs: %d  bsigs: %d\n",
			p->ntups, p->ntsigs, p->npsigs, p->nbsigs);
    printf("  #pages:  tuples: %d  tsigs: %d  psigs: %d  bsigs: %d\n",
			p->npages, p->tsigNpages, p->psigNpages, p->bsigNpages);
	printf("Static:\n");
    printf("  tups   #attrs: %d  size: %d bytes  max/page: %d\n",
			p->nattrs, p->tupsize, p->tupPP);
	printf("  sigs   bits/attr: %d\n", p->tk);
	printf("  tsigs  size: %d bits (%d bytes)  max/page: %d\n",
			p->tm, p->tsigSize, p->tsigPP);
	printf("  psigs  size: %d bits (%d bytes)  max/page: %d\n",
			p->pm, p->psigSize, p->psigPP);
	printf("  bsigs  size: %d bits (%d bytes)  max/page: %d\n",
			p->bm, p->bsigSize, p->bsigPP);
}
