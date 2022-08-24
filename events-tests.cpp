/*
    g++ -std=c++11 -O0 -g events-tests.cpp -o events-tests
*/

#include <iostream>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#include <list>
#include <memory>
#include <vector>
#include <mutex>
#include <algorithm>

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>

#include "tinyformat.h"
#include "mem_read.h"
#include "events-tests.h"
#include "utilstrencodings.h"

#define portable_mutex_lock pthread_mutex_lock
#define portable_mutex_unlock pthread_mutex_unlock
#define KOMODO_ASSETCHAIN_MAXLEN 65 /* util.h, bitcoind.cpp, komodo_defs.h, komodo_globals.h, komodo_structs.h */
char ASSETCHAINS_SYMBOL[] = {0};
// char ASSETCHAINS_SYMBOL[] = {'M','C','L', 0};

int32_t KOMODO_EXTERNAL_NOTARIES = 0; /* komodo_globals.h */
int32_t KOMODO_LASTMINED,prevKOMODO_LASTMINED; /* komodo_globals.h */

bool IS_KOMODO_NOTARY = false;

union _bits256 {

    uint8_t bytes[32] = {}; uint16_t ushorts[16]; uint32_t uints[8]; uint64_t ulongs[4];
    uint64_t txid;

    std::string ToString() const
    {
        std::string res = "";
        for (int i=0; i<32; i++)
            res = res + strprintf("%02x", bytes[i]);
        return res;
    }

    std::string ToStringRev() const
    {
        std::string res = "";
        for (int i=0; i<32; i++)
            res = strprintf("%02x", bytes[i]) + res;
        return res;
    }

    void SetNull() {
        for (int i=0; i<32; i++) bytes[i] = 0;
    }
};

typedef union _bits256 bits256;
typedef union _bits256 uint256;

bool operator==(const _bits256& lhs, const _bits256& rhs) {
    if (lhs.ulongs[0] != rhs.ulongs[0] ||
        lhs.ulongs[1] != rhs.ulongs[1] ||
        lhs.ulongs[2] != rhs.ulongs[2] ||
        lhs.ulongs[3] != rhs.ulongs[3])
    return false;
    return true;
}

inline bool operator!=(const uint256& lhs, const uint256& rhs) { return !(lhs == rhs); }

typedef int64_t CAmount;
static const CAmount COIN = 100000000; // amount.h
static const CAmount CENT = 1000000;

/* komodo_structs.h */
#define KOMODO_EVENT_RATIFY 'P'
#define KOMODO_EVENT_NOTARIZED 'N'
#define KOMODO_EVENT_KMDHEIGHT 'K'
#define KOMODO_EVENT_REWIND 'B'
#define KOMODO_EVENT_PRICEFEED 'V'
#define KOMODO_EVENT_OPRETURN 'R'
#define KOMODO_OPRETURN_DEPOSIT 'D'
#define KOMODO_OPRETURN_ISSUED 'I' // assetchain
#define KOMODO_OPRETURN_WITHDRAW 'W' // assetchain
#define KOMODO_OPRETURN_REDEEMED 'X'

namespace events_old {

    int32_t rewind_count = 0;
    pthread_mutex_t komodo_mutex,staked_mutex;

    /*
    int g() {
        return 777;
    }
    void f() {
        std::cout << g() << std::endl;
    }
    */

    struct notarized_checkpoint /* komodo_structs.h */
    {
        uint256 notarized_hash,notarized_desttxid,MoM,MoMoM;
        int32_t nHeight,notarized_height,MoMdepth,MoMoMdepth,MoMoMoffset,kmdstarti,kmdendi;
    };

    struct komodo_event_notarized { uint256 blockhash,desttxid,MoM; int32_t notarizedheight,MoMdepth; char dest[16]; };
    struct komodo_event_pubkeys { uint8_t num; uint8_t pubkeys[64][33]; };
    struct komodo_event_opreturn { uint256 txid; uint64_t value; uint16_t vout,oplen; uint8_t opret[]; };
    struct komodo_event_pricefeed { uint8_t num; uint32_t prices[35]; };

    struct komodo_event
    {
        struct komodo_event *related;
        uint16_t len;
        int32_t height;
        uint8_t type,reorged;
        char symbol[KOMODO_ASSETCHAIN_MAXLEN];
        uint8_t space[];
    };

    struct komodo_state
    {
        uint256 NOTARIZED_HASH,NOTARIZED_DESTTXID,MoM;
        int32_t SAVEDHEIGHT,CURRENT_HEIGHT,NOTARIZED_HEIGHT,MoMdepth;
        uint32_t SAVEDTIMESTAMP;
        uint64_t deposited,issued,withdrawn,approved,redeemed,shorted;
        struct notarized_checkpoint *NPOINTS;
        int32_t NUM_NPOINTS,last_NPOINTSi;
        struct komodo_event **Komodo_events; int32_t Komodo_numevents;
        uint32_t RTbufs[64][3]; uint64_t RTmask;
    };

    struct komodo_event *komodo_eventadd(struct komodo_state *sp,int32_t height,char *symbol,uint8_t type,uint8_t *data,uint16_t datalen)
    {
        struct komodo_event *ep=0; uint16_t len = (uint16_t)(sizeof(*ep) + datalen);

        if ( sp != 0 /*&& ASSETCHAINS_SYMBOL[0] != 0*/ ) // DISABLED for debug purposes (!)
        {
            portable_mutex_lock(&komodo_mutex);
            ep = (struct komodo_event *)calloc(1,len);
            ep->len = len;
            ep->height = height;
            ep->type = type;
            strcpy(ep->symbol,symbol);
            if ( datalen != 0 )
                memcpy(ep->space,data,datalen);
            sp->Komodo_events = (struct komodo_event **)realloc(sp->Komodo_events,(1 + sp->Komodo_numevents) * sizeof(*sp->Komodo_events));
            sp->Komodo_events[sp->Komodo_numevents++] = ep;

            // static int32_t komodo_eventadd_call_count;
            // std::cerr << __func__ << " " << ++komodo_eventadd_call_count << " " << sp->Komodo_numevents << std::endl;
            // if (++komodo_eventadd_call_count == 4635) {
            //     std::cerr << "after exit from this func, komodo_event_rewind will called and sp->Komodo_numevents will be decreased " << std::endl;
            // }

            portable_mutex_unlock(&komodo_mutex);
        }
        return(ep);
    }

    void komodo_eventadd_pubkeys(struct komodo_state *sp,char *symbol,int32_t height,uint8_t num,uint8_t pubkeys[64][33])
    {
        struct komodo_event_pubkeys P;
        //printf("eventadd pubkeys ht.%d\n",height);
        memset(&P,0,sizeof(P));
        P.num = num;
        memcpy(P.pubkeys,pubkeys,33 * num);
        komodo_eventadd(sp,height,symbol,KOMODO_EVENT_RATIFY,(uint8_t *)&P,(int32_t)(sizeof(P.num) + 33 * num));
        /* if ( sp != 0 )
            komodo_notarysinit(height,pubkeys,num); */
    }

    /* komodo_notary.h */
    void komodo_notarized_update(struct komodo_state *sp,int32_t nHeight,int32_t notarized_height,uint256 notarized_hash,uint256 notarized_desttxid,uint256 MoM,int32_t MoMdepth)
    {
        struct notarized_checkpoint *np;
        if ( notarized_height >= nHeight )
        {
            fprintf(stderr,"komodo_notarized_update REJECT notarized_height %d > %d nHeight\n",notarized_height,nHeight);
            return;
        }
        if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 )
            fprintf(stderr,"[%s] komodo_notarized_update nHeight.%d notarized_height.%d\n",ASSETCHAINS_SYMBOL,nHeight,notarized_height);
        portable_mutex_lock(&komodo_mutex);
        sp->NPOINTS = (struct notarized_checkpoint *)realloc(sp->NPOINTS,(sp->NUM_NPOINTS+1) * sizeof(*sp->NPOINTS));
        np = &sp->NPOINTS[sp->NUM_NPOINTS++];
        memset(np,0,sizeof(*np));
        np->nHeight = nHeight;
        sp->NOTARIZED_HEIGHT = np->notarized_height = notarized_height;
        sp->NOTARIZED_HASH = np->notarized_hash = notarized_hash;
        sp->NOTARIZED_DESTTXID = np->notarized_desttxid = notarized_desttxid;
        sp->MoM = np->MoM = MoM;
        sp->MoMdepth = np->MoMdepth = MoMdepth;
        portable_mutex_unlock(&komodo_mutex);
    }

    void komodo_eventadd_notarized(struct komodo_state *sp,char *symbol,int32_t height,char *dest,uint256 notarized_hash,uint256 notarized_desttxid,int32_t notarizedheight,uint256 MoM,int32_t MoMdepth)
    {
        static uint32_t counter; int32_t verified=0; char *coin; struct komodo_event_notarized N;
        coin = (ASSETCHAINS_SYMBOL[0] == 0) ? (char *)"KMD" : ASSETCHAINS_SYMBOL;
        /*
        if ( IS_KOMODO_NOTARY && (verified= komodo_verifynotarization(symbol,dest,height,notarizedheight,notarized_hash,notarized_desttxid)) < 0 )
        {
            if ( counter++ < 100 )
                printf("[%s] error validating notarization ht.%d notarized_height.%d, if on a pruned %s node this can be ignored\n",ASSETCHAINS_SYMBOL,height,notarizedheight,dest);
        }
        else
        */
        if ( strcmp(symbol,coin) == 0 )
        {
            if ( 0 && IS_KOMODO_NOTARY && verified != 0 )
                fprintf(stderr,"validated [%s] ht.%d notarized %d\n",coin,height,notarizedheight);
            memset(&N,0,sizeof(N));
            N.blockhash = notarized_hash;
            N.desttxid = notarized_desttxid;
            N.notarizedheight = notarizedheight;
            N.MoM = MoM;
            N.MoMdepth = MoMdepth;
            strncpy(N.dest,dest,sizeof(N.dest)-1);
            komodo_eventadd(sp,height,symbol,KOMODO_EVENT_NOTARIZED,(uint8_t *)&N,sizeof(N));
            if ( sp != 0 )
                komodo_notarized_update(sp,height,notarizedheight,notarized_hash,notarized_desttxid,MoM,MoMdepth);
        }
    }

    void komodo_setkmdheight(struct komodo_state *sp,int32_t kmdheight,uint32_t timestamp)
    {
        if ( sp != 0 )
        {
            if ( kmdheight > sp->SAVEDHEIGHT )
            {
                sp->SAVEDHEIGHT = kmdheight;
                sp->SAVEDTIMESTAMP = timestamp;
            }
            if ( kmdheight > sp->CURRENT_HEIGHT )
                sp->CURRENT_HEIGHT = kmdheight;
        }
    }

    void komodo_event_undo(struct komodo_state *sp,struct komodo_event *ep)
    {
        switch ( ep->type )
        {
            case KOMODO_EVENT_RATIFY: printf("rewind of ratify, needs to be coded.%d\n",ep->height); break;
            case KOMODO_EVENT_NOTARIZED: break;
            case KOMODO_EVENT_KMDHEIGHT:
                if ( ep->height <= sp->SAVEDHEIGHT )
                    sp->SAVEDHEIGHT = ep->height;
                break;
            case KOMODO_EVENT_PRICEFEED:
                // backtrack prices;
                break;
            case KOMODO_EVENT_OPRETURN:
                // backtrack opreturns
                break;
        }
    }

    void komodo_event_rewind(struct komodo_state *sp,char *symbol,int32_t height)
    {
        struct komodo_event *ep;
        if ( sp != 0 )
        {
            if ( ASSETCHAINS_SYMBOL[0] == 0 && height <= KOMODO_LASTMINED && prevKOMODO_LASTMINED != 0 )
            {
                printf("undo KOMODO_LASTMINED %d <- %d\n",KOMODO_LASTMINED,prevKOMODO_LASTMINED);
                KOMODO_LASTMINED = prevKOMODO_LASTMINED;
                prevKOMODO_LASTMINED = 0;
            }
            rewind_count++;
            while ( sp->Komodo_events != 0 && sp->Komodo_numevents > 0 )
            {
                if ( (ep= sp->Komodo_events[sp->Komodo_numevents-1]) != 0 )
                {
                    if ( ep->height < height )
                        break;
                    //printf("[%s] undo %s event.%c ht.%d for rewind.%d\n",ASSETCHAINS_SYMBOL,symbol,ep->type,ep->height,height);
                    komodo_event_undo(sp,ep);
                    sp->Komodo_numevents--;
                }
            }
        }
    }
    /* komodo_events.h */
    void komodo_eventadd_kmdheight(struct komodo_state *sp,char *symbol,int32_t height,int32_t kmdheight,uint32_t timestamp)
    {
        // static int32_t komodo_eventadd_kmdheight_call_count;
        // std::cerr << __func__ << " [" << sp->Komodo_numevents << "] " << ++komodo_eventadd_kmdheight_call_count << " " << height << " " << kmdheight << " " << timestamp << std::endl;

        uint32_t buf[2];
        if ( kmdheight > 0 )
        {
            buf[0] = (uint32_t)kmdheight;
            buf[1] = timestamp;
            komodo_eventadd(sp,height,symbol,KOMODO_EVENT_KMDHEIGHT,(uint8_t *)buf,sizeof(buf));
            if ( sp != 0 )
                komodo_setkmdheight(sp,kmdheight,timestamp);
        }
        else
        {
            //fprintf(stderr,"REWIND kmdheight.%d\n",kmdheight);
            kmdheight = -kmdheight;
            komodo_eventadd(sp,height,symbol,KOMODO_EVENT_REWIND,(uint8_t *)&height,sizeof(height));
            if ( sp != 0 )
                komodo_event_rewind(sp,symbol,height);
        }
    }

    /* komodo_gateway.cpp */
    const char *komodo_opreturn(int32_t height,uint64_t value,uint8_t *opretbuf,int32_t opretlen,uint256 txid,uint16_t vout,char *source)
    {
        const char *typestr = "unknown";
        return typestr;
    }

    void komodo_eventadd_opreturn(struct komodo_state *sp,char *symbol,int32_t height,uint256 txid,uint64_t value,uint16_t vout,uint8_t *buf,uint16_t opretlen)
    {
        struct komodo_event_opreturn O; uint8_t *opret;
        if ( ASSETCHAINS_SYMBOL[0] != 0 )
        {
            opret = (uint8_t *)calloc(1,sizeof(O) + opretlen + 16);
            O.txid = txid;
            O.value = value;
            O.vout = vout;
            memcpy(opret,&O,sizeof(O));
            memcpy(&opret[sizeof(O)],buf,opretlen);
            O.oplen = (int32_t)(opretlen + sizeof(O));
            komodo_eventadd(sp,height,symbol,KOMODO_EVENT_OPRETURN,opret,O.oplen);
            free(opret);
            if ( sp != 0 )
                komodo_opreturn(height,value,buf,opretlen,txid,vout,symbol);
        }
    }

    void komodo_eventadd_pricefeed(struct komodo_state *sp,char *symbol,int32_t height,uint32_t *prices,uint8_t num)
    {
        struct komodo_event_pricefeed F;
        if ( num == sizeof(F.prices)/sizeof(*F.prices) )
        {
            memset(&F,0,sizeof(F));
            F.num = num;
            memcpy(F.prices,prices,sizeof(*F.prices) * num);
            komodo_eventadd(sp,height,symbol,KOMODO_EVENT_PRICEFEED,(uint8_t *)&F,(int32_t)(sizeof(F.num) + sizeof(*F.prices) * num));
            /*if ( sp != 0 )
                komodo_pvals(height,prices,num);*/
        } //else fprintf(stderr,"skip pricefeed[%d]\n",num);
    }

    /**
     * @brief
     *
     * @param sp
     * @param fp
     * @param symbol
     * @param dest
     * @return int32_t
     */
    int32_t komodo_parsestatefile(struct komodo_state *sp,FILE *fp,char *symbol,char *dest)
    {
        static int32_t errs;
        int32_t func,ht,notarized_height,num,matched=0,MoMdepth; uint256 MoM,notarized_hash,notarized_desttxid; uint8_t pubkeys[64][33];
        if ( (func= fgetc(fp)) != EOF )
        {
            if ( ASSETCHAINS_SYMBOL[0] == 0 && strcmp(symbol,"KMD") == 0 )
                matched = 1;
            else matched = (strcmp(symbol,ASSETCHAINS_SYMBOL) == 0);
            if ( fread(&ht,1,sizeof(ht),fp) != sizeof(ht) )
                errs++;
            // if (ht == 2441413) {
            //          std::cerr << "[new] Here we should break ... " << std::endl;
            // }
            if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 && func != 'T' )
                printf("[%s] matched.%d fpos.%ld func.(%d %c) ht.%d\n",ASSETCHAINS_SYMBOL,matched,ftell(fp),func,func,ht);
            if ( func == 'P' )
            {
                if ( (num= fgetc(fp)) <= 64 )
                {
                    if ( fread(pubkeys,33,num,fp) != num )
                        errs++;
                    else
                    {
                        //printf("updated %d pubkeys at %s ht.%d\n",num,symbol,ht);
                        if ( (KOMODO_EXTERNAL_NOTARIES != 0 && matched != 0) || (strcmp(symbol,"KMD") == 0 && KOMODO_EXTERNAL_NOTARIES == 0) )
                            komodo_eventadd_pubkeys(sp,symbol,ht,num,pubkeys);
                    }
                } else printf("illegal num.%d\n",num);
            }
            else if ( func == 'N' || func == 'M' )
            {
                if ( fread(&notarized_height,1,sizeof(notarized_height),fp) != sizeof(notarized_height) )
                    errs++;
                if ( fread(&notarized_hash,1,sizeof(notarized_hash),fp) != sizeof(notarized_hash) )
                    errs++;
                if ( fread(&notarized_desttxid,1,sizeof(notarized_desttxid),fp) != sizeof(notarized_desttxid) )
                    errs++;
                if ( func == 'M' )
                {
                    if ( fread(&MoM,1,sizeof(MoM),fp) != sizeof(MoM) )
                        errs++;
                    if ( fread(&MoMdepth,1,sizeof(MoMdepth),fp) != sizeof(MoMdepth) )
                        errs++;
                    if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 && sp != 0 )
                        printf("%s load[%s.%d -> %s] NOTARIZED %d %s MoM.%s %d CCid.%u\n",ASSETCHAINS_SYMBOL,symbol,sp->NUM_NPOINTS,dest,notarized_height,notarized_hash.ToString().c_str(),MoM.ToString().c_str(),MoMdepth&0xffff,(MoMdepth>>16)&0xffff);
                }
                else
                {
                    memset(&MoM,0,sizeof(MoM));
                    MoMdepth = 0;
                }
                //if ( matched != 0 ) global independent states -> inside *sp
                komodo_eventadd_notarized(sp,symbol,ht,dest,notarized_hash,notarized_desttxid,notarized_height,MoM,MoMdepth);
            }
            else if ( func == 'U' ) // deprecated
            {
                uint8_t n,nid; uint256 hash; uint64_t mask;
                n = fgetc(fp);
                nid = fgetc(fp);
                //printf("U %d %d\n",n,nid);
                if ( fread(&mask,1,sizeof(mask),fp) != sizeof(mask) )
                    errs++;
                if ( fread(&hash,1,sizeof(hash),fp) != sizeof(hash) )
                    errs++;
                //if ( matched != 0 )
                //    komodo_eventadd_utxo(sp,symbol,ht,nid,hash,mask,n);
            }
            else if ( func == 'K' )
            {
                int32_t kheight;
                if ( fread(&kheight,1,sizeof(kheight),fp) != sizeof(kheight) )
                    errs++;
                //if ( matched != 0 ) global independent states -> inside *sp
                //printf("%s.%d load[%s] ht.%d\n",ASSETCHAINS_SYMBOL,ht,symbol,kheight);
                komodo_eventadd_kmdheight(sp,symbol,ht,kheight,0);
            }
            else if ( func == 'T' )
            {
                int32_t kheight,ktimestamp;
                if ( fread(&kheight,1,sizeof(kheight),fp) != sizeof(kheight) )
                    errs++;
                if ( fread(&ktimestamp,1,sizeof(ktimestamp),fp) != sizeof(ktimestamp) )
                    errs++;
                //if ( matched != 0 ) global independent states -> inside *sp
                //printf("%s.%d load[%s] ht.%d t.%u\n",ASSETCHAINS_SYMBOL,ht,symbol,kheight,ktimestamp);
                komodo_eventadd_kmdheight(sp,symbol,ht,kheight,ktimestamp);
            }
            else if ( func == 'R' )
            {
                uint16_t olen,v; uint64_t ovalue; uint256 txid; uint8_t opret[16384*4];
                if ( fread(&txid,1,sizeof(txid),fp) != sizeof(txid) )
                    errs++;
                if ( fread(&v,1,sizeof(v),fp) != sizeof(v) )
                    errs++;
                if ( fread(&ovalue,1,sizeof(ovalue),fp) != sizeof(ovalue) )
                    errs++;
                if ( fread(&olen,1,sizeof(olen),fp) != sizeof(olen) )
                    errs++;
                if ( olen < sizeof(opret) )
                {
                    if ( fread(opret,1,olen,fp) != olen )
                        errs++;
                    if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 && matched != 0 )
                    {
                        int32_t i;  for (i=0; i<olen; i++)
                            printf("%02x",opret[i]);
                        printf(" %s.%d load[%s] opret[%c] len.%d %.8f\n",ASSETCHAINS_SYMBOL,ht,symbol,opret[0],olen,(double)ovalue/COIN);
                    }
                    komodo_eventadd_opreturn(sp,symbol,ht,txid,ovalue,v,opret,olen); // global shared state -> global PAX
                } else
                {
                    int32_t i;
                    for (i=0; i<olen; i++)
                        fgetc(fp);
                    //printf("illegal olen.%u\n",olen);
                }
            }
            else if ( func == 'D' )
            {
                printf("unexpected function D[%d]\n",ht);
            }
            else if ( func == 'V' )
            {
                int32_t numpvals; uint32_t pvals[128];
                numpvals = fgetc(fp);
                if ( numpvals*sizeof(uint32_t) <= sizeof(pvals) && fread(pvals,sizeof(uint32_t),numpvals,fp) == numpvals )
                {
                    //if ( matched != 0 ) global shared state -> global PVALS
                    //printf("%s load[%s] prices %d\n",ASSETCHAINS_SYMBOL,symbol,ht);
                    komodo_eventadd_pricefeed(sp,symbol,ht,pvals,numpvals);
                    //printf("load pvals ht.%d numpvals.%d\n",ht,numpvals);
                } else printf("error loading pvals[%d]\n",numpvals);
            } // else printf("[%s] %s illegal func.(%d %c)\n",ASSETCHAINS_SYMBOL,symbol,func,func);
            return(func);
        } else return(-1);
    }


}

namespace komodo {
    /* komodo_structs.h */
    enum komodo_event_type
    {
        EVENT_PUBKEYS,
        EVENT_NOTARIZED,
        EVENT_U,
        EVENT_KMDHEIGHT,
        EVENT_OPRETURN,
        EVENT_PRICEFEED,
        EVENT_REWIND
    };

    /***
     * base class for events
     */
    class event
    {
    public:
        event(komodo_event_type t, int32_t height) : type(t), height(height) {}
        virtual ~event() = default;
        komodo_event_type type;
        int32_t height;
    };

    struct event_rewind : public event
    {
        event_rewind() : event(komodo_event_type::EVENT_REWIND, 0) {}
        event_rewind(int32_t ht) : event(EVENT_REWIND, ht) {}
        event_rewind(uint8_t* data, long &pos, long data_len, int32_t height);
    };

    struct event_notarized : public event
    {
        event_notarized() : event(EVENT_NOTARIZED, 0), notarizedheight(0), MoMdepth(0) {
            memset(this->dest, 0, sizeof(this->dest));
        }
        event_notarized(int32_t ht, const char* _dest) : event(EVENT_NOTARIZED, ht), notarizedheight(0), MoMdepth(0) {
            strncpy(this->dest, _dest, sizeof(this->dest)-1); this->dest[sizeof(this->dest)-1] = 0;
        }
        event_notarized(uint8_t* data, long &pos, long data_len, int32_t height, const char* _dest, bool includeMoM = false);
        event_notarized(FILE* fp, int32_t ht, const char* _dest, bool includeMoM = false);
        uint256 blockhash;
        uint256 desttxid;
        uint256 MoM;
        int32_t notarizedheight;
        int32_t MoMdepth;
        char dest[16];
    };

    std::ostream& operator<<(std::ostream& os, const event_notarized& in);

    struct event_pubkeys : public event
    {
        /***
         * Default ctor
         */
        event_pubkeys() : event(EVENT_PUBKEYS, 0), num(0)
        {
            memset(pubkeys, 0, 64 * 33);
        }
        event_pubkeys(int32_t ht) : event(EVENT_PUBKEYS, ht), num(0)
        {
            memset(pubkeys, 0, 64 * 33);
        }
        /***
         * ctor from data stream
         * @param data the data stream
         * @param pos the starting position (will advance)
         * @param data_len full length of data
         */
        event_pubkeys(uint8_t* data, long &pos, long data_len, int32_t height);
        event_pubkeys(FILE* fp, int32_t height);
        uint8_t num = 0;
        uint8_t pubkeys[64][33];
    };
    std::ostream& operator<<(std::ostream& os, const event_pubkeys& in);

    // (???)
    struct event_u : public event
    {
        event_u() : event(EVENT_U, 0)
        {
            memset(mask, 0, 8);
            memset(hash, 0, 32);
        }
        event_u(int32_t ht) : event(EVENT_U, ht)
        {
            memset(mask, 0, 8);
            memset(hash, 0, 32);
        }
        event_u(uint8_t *data, long &pos, long data_len, int32_t height);
        event_u(FILE* fp, int32_t height);
        uint8_t n = 0;
        uint8_t nid = 0;
        uint8_t mask[8];
        uint8_t hash[32];
    };
    std::ostream& operator<<(std::ostream& os, const event_u& in);

    struct event_kmdheight : public event
    {
        event_kmdheight() : event(EVENT_KMDHEIGHT, 0) {}
        event_kmdheight(int32_t ht) : event(EVENT_KMDHEIGHT, ht) {}
        event_kmdheight(uint8_t *data, long &pos, long data_len, int32_t height, bool includeTimestamp = false);
        event_kmdheight(FILE* fp, int32_t height, bool includeTimestamp = false);
        int32_t kheight = 0;
        uint32_t timestamp = 0;
    };

    struct event_opreturn : public event
    {
        event_opreturn() : event(EVENT_OPRETURN, 0)
        {
            txid.SetNull();
        }
        event_opreturn(int32_t ht) : event(EVENT_OPRETURN, ht)
        {
            txid.SetNull();
        }
        event_opreturn(uint8_t *data, long &pos, long data_len, int32_t height);
        event_opreturn(FILE* fp, int32_t height);
        uint256 txid;
        uint16_t vout = 0;
        uint64_t value = 0;
        std::vector<uint8_t> opret;
    };

    struct event_pricefeed : public event
    {
        event_pricefeed() : event(EVENT_PRICEFEED, 0), num(0)
        {
            memset(prices, 0, 35);
        }
        event_pricefeed(int32_t ht) : event(EVENT_PRICEFEED, ht)
        {
            memset(prices, 0, 35);
        }
        event_pricefeed(uint8_t *data, long &pos, long data_len, int32_t height);
        event_pricefeed(FILE* fp, int32_t height);
        uint8_t num = 0;
        uint32_t prices[35];
    };

    /* komodo_structs.cpp */

    event_pubkeys::event_pubkeys(uint8_t* data, long& pos, long data_len, int32_t height) : event(EVENT_PUBKEYS, height)
    {
        num = data[pos++];
        if (num > 64)
            throw parse_error("Illegal number of keys: " + std::to_string(num));
        mem_nread(pubkeys, num, data, pos, data_len);
    }

    event_pubkeys::event_pubkeys(FILE* fp, int32_t height) : event(EVENT_PUBKEYS, height)
    {
        num = fgetc(fp);
        if ( fread(pubkeys,33,num,fp) != num )
            throw parse_error("Illegal number of keys: " + std::to_string(num));
    }

    // event_notarized::event_notarized(uint8_t *data, long &pos, long data_len, int32_t height, const char* _dest, bool includeMoM)
    //         : event(EVENT_NOTARIZED, height), MoMdepth(0)
    event_notarized::event_notarized(uint8_t *data, long &pos, long data_len, int32_t height, 
        const char* _dest, bool includeMoM)
        : event_notarized(height, dest)
    {
        strncpy(this->dest, _dest, sizeof(this->dest)-1);
        this->dest[sizeof(this->dest)-1] = 0;
        MoM.SetNull();
        mem_read(this->notarizedheight, data, pos, data_len);
        mem_read(this->blockhash, data, pos, data_len);
        mem_read(this->desttxid, data, pos, data_len);
        if (includeMoM)
        {
            mem_read(this->MoM, data, pos, data_len);
            mem_read(this->MoMdepth, data, pos, data_len);
        }
    }

    event_notarized::event_notarized(FILE* fp, int32_t height, const char* _dest, bool includeMoM)
            : event(EVENT_NOTARIZED, height), MoMdepth(0)
    {
        strncpy(this->dest, _dest, sizeof(this->dest)-1);
        this->dest[sizeof(this->dest)-1] = 0;
        MoM.SetNull();
        if ( fread(&notarizedheight,1,sizeof(notarizedheight),fp) != sizeof(notarizedheight) )
            throw parse_error("Invalid notarization height");
        if ( fread(&blockhash,1,sizeof(blockhash),fp) != sizeof(blockhash) )
            throw parse_error("Invalid block hash");
        if ( fread(&desttxid,1,sizeof(desttxid),fp) != sizeof(desttxid) )
            throw parse_error("Invalid Destination TXID");
        if ( includeMoM )
        {
            if ( fread(&MoM,1,sizeof(MoM),fp) != sizeof(MoM) )
                throw parse_error("Invalid MoM");
            if ( fread(&MoMdepth,1,sizeof(MoMdepth),fp) != sizeof(MoMdepth) )
                throw parse_error("Invalid MoMdepth");
        }
    }

    /***
     * Helps serialize integrals
     */
    template<class T>
    class serializable
    {
    public:
        serializable(const T& in) : value(in) {}
        T value;
    };

    std::ostream& operator<<(std::ostream& os, serializable<int32_t> in)
    {
        os  << (uint8_t) (in.value & 0x000000ff)
            << (uint8_t) ((in.value & 0x0000ff00) >> 8)
            << (uint8_t) ((in.value & 0x00ff0000) >> 16)
            << (uint8_t) ((in.value & 0xff000000) >> 24);
        return os;
    }

    std::ostream& operator<<(std::ostream& os, serializable<uint16_t> in)
    {
        os  << (uint8_t)  (in.value & 0x00ff)
            << (uint8_t) ((in.value & 0xff00) >> 8);
        return os;
    }

    std::ostream& operator<<(std::ostream& os, serializable<uint256> in)
    {
        // in.value.Serialize(os);
        for (int i = 0; i < 32; ++i) {
            os << in.value.bytes[i];
        }
        return os;
    }

    std::ostream& operator<<(std::ostream& os, serializable<uint64_t> in)
    {
        os  << (uint8_t)  (in.value & 0x00000000000000ff)
            << (uint8_t) ((in.value & 0x000000000000ff00) >> 8)
            << (uint8_t) ((in.value & 0x0000000000ff0000) >> 16)
            << (uint8_t) ((in.value & 0x00000000ff000000) >> 24)
            << (uint8_t) ((in.value & 0x000000ff00000000) >> 32)
            << (uint8_t) ((in.value & 0x0000ff0000000000) >> 40)
            << (uint8_t) ((in.value & 0x00ff000000000000) >> 48)
            << (uint8_t) ((in.value & 0xff00000000000000) >> 56);
        return os;
    }

    /****
     * This serializes the 5 byte header of an event
     * @param os the stream
     * @param in the event
     * @returns the stream
     */
    std::ostream& operator<<(std::ostream& os, const event& in)
    {
        switch (in.type)
        {
            case(EVENT_PUBKEYS):
                os << "P";
                break;
            case(EVENT_NOTARIZED):
            {
                event_notarized* tmp = dynamic_cast<event_notarized*>(const_cast<event*>(&in));
                if (tmp->MoMdepth == 0)
                    os << "N";
                else
                    os << "M";
                break;
            }
            case(EVENT_U):
                os << "U";
                break;
            case(EVENT_KMDHEIGHT):
            {
                event_kmdheight* tmp = dynamic_cast<event_kmdheight*>(const_cast<event*>(&in));
                if (tmp->timestamp == 0)
                    os << "K";
                else
                    os << "T";
                break;
            }
            case(EVENT_OPRETURN):
                os << "R";
                break;
            case(EVENT_PRICEFEED):
                os << "V";
                break;
            case(EVENT_REWIND):
                os << "B";
                break;
        }
        os << serializable<int32_t>(in.height);
        return os;
    }

    std::ostream& operator<<(std::ostream& os, const event_notarized& in)
    {
        const event& e = dynamic_cast<const event&>(in);
        os << e;
        os << serializable<int32_t>(in.notarizedheight);
        os << serializable<uint256>(in.blockhash);
        os << serializable<uint256>(in.desttxid);
        if (in.MoMdepth > 0)
        {
            os << serializable<uint256>(in.MoM);
            os << serializable<int32_t>(in.MoMdepth);
        }
        return os;
    }

    std::ostream& operator<<(std::ostream& os, const event_pubkeys& in)
    {
        const event& e = static_cast<const event&>(in);
        os << e;
        os << in.num;
        for(uint8_t i = 0; i < in.num; ++i)
            for(uint8_t j = 0; j < 33; ++j)
                os << in.pubkeys[i][j];
        return os;
    }

    std::ostream& operator<<(std::ostream& os, const event_u& in)
    {
        const event& e = dynamic_cast<const event&>(in);
        os << e;
        os << in.n << in.nid;
        os.write((const char*)in.mask, 8);
        os.write((const char*)in.hash, 32);
        return os;
    }

    event_u::event_u(uint8_t *data, long &pos, long data_len, int32_t height) : event(EVENT_U, height)
    {
        mem_read(this->n, data, pos, data_len);
        mem_read(this->nid, data, pos, data_len);
        mem_read(this->mask, data, pos, data_len);
        mem_read(this->hash, data, pos, data_len);
    }

    event_u::event_u(FILE *fp, int32_t height) : event(EVENT_U, height)
    {
        if (fread(&n, 1, sizeof(n), fp) != sizeof(n))
            throw parse_error("Unable to read n of event U from file");
        if (fread(&nid, 1, sizeof(nid), fp) != sizeof(n))
            throw parse_error("Unable to read nid of event U from file");
        if (fread(&mask, 1, sizeof(mask), fp) != sizeof(mask))
            throw parse_error("Unable to read mask of event U from file");
        if (fread(&hash, 1, sizeof(hash), fp) != sizeof(hash))
            throw parse_error("Unable to read hash of event U from file");
    }

    event_kmdheight::event_kmdheight(uint8_t* data, long &pos, long data_len, int32_t height, bool includeTimestamp) : event(EVENT_KMDHEIGHT, height)
    {
        mem_read(this->kheight, data, pos, data_len);
        if (includeTimestamp)
            mem_read(this->timestamp, data, pos, data_len);
    }

    event_kmdheight::event_kmdheight(FILE *fp, int32_t height, bool includeTimestamp) : event(EVENT_KMDHEIGHT, height)
    {
        if ( fread(&kheight,1,sizeof(kheight),fp) != sizeof(kheight) )
            throw parse_error("Unable to parse KMD height");
        if (includeTimestamp)
        {
            if ( fread( &timestamp, 1, sizeof(timestamp), fp) != sizeof(timestamp) )
                throw parse_error("Unable to parse timestamp of KMD height");
        }
    }

    event_opreturn::event_opreturn(uint8_t *data, long &pos, long data_len, int32_t height) : event(EVENT_OPRETURN, height)
    {
        mem_read(this->txid, data, pos, data_len);
        mem_read(this->vout, data, pos, data_len);
        mem_read(this->value, data, pos, data_len);
        uint16_t oplen;
        mem_read(oplen, data, pos, data_len);
        if (oplen <= data_len - pos)
        {
            this->opret = std::vector<uint8_t>( &data[pos], &data[pos] + oplen);
            pos += oplen;
        }
    }

    event_opreturn::event_opreturn(FILE* fp, int32_t height) : event(EVENT_OPRETURN, height)
    {
        if ( fread(&txid,1,sizeof(txid),fp) != sizeof(txid) )
            throw parse_error("Unable to parse txid of opreturn record");
        if ( fread(&vout,1,sizeof(vout),fp) != sizeof(vout) )
            throw parse_error("Unable to parse vout of opreturn record");
        if ( fread(&value,1,sizeof(value),fp) != sizeof(value) )
            throw parse_error("Unable to parse value of opreturn record");
        uint16_t oplen;
        if ( fread(&oplen,1,sizeof(oplen),fp) != sizeof(oplen) )
            throw parse_error("Unable to parse length of opreturn record");
        std::unique_ptr<uint8_t> b(new uint8_t[oplen]);
        if ( fread(b.get(), 1, oplen, fp) != oplen)
            throw parse_error("Unable to parse binary data of opreturn");
        this->opret = std::vector<uint8_t>( b.get(), b.get() + oplen);
    }

    event_pricefeed::event_pricefeed(uint8_t *data, long &pos, long data_len, int32_t height) : event(EVENT_PRICEFEED, height)
    {
        mem_read(this->num, data, pos, data_len);
        // we're only interested if there are 35 prices.
        // If there is any other amount, advance the pointer, but don't save it
        if (this->num == 35)
            mem_nread(this->prices, this->num, data, pos, data_len);
        else
            pos += num * sizeof(uint32_t);
    }

    event_pricefeed::event_pricefeed(FILE* fp, int32_t height) : event(EVENT_PRICEFEED, height)
    {
        num = fgetc(fp);
        if ( num * sizeof(uint32_t) <= sizeof(prices) && fread(prices,sizeof(uint32_t),num,fp) != num )
            throw parse_error("Unable to parse price feed");
    }


} // namespace komodo

namespace events_new {

int32_t rewind_count = 0;
std::mutex komodo_mutex; /* komodo_globals.h */

struct notarized_checkpoint /* komodo_structs.h */
    {
        uint256 notarized_hash,notarized_desttxid,MoM,MoMoM;
        int32_t nHeight,notarized_height,MoMdepth,MoMoMdepth,MoMoMoffset,kmdstarti,kmdendi;
    };

    struct komodo_event_notarized { uint256 blockhash,desttxid,MoM; int32_t notarizedheight,MoMdepth; char dest[16]; };
    struct komodo_event_pubkeys { uint8_t num; uint8_t pubkeys[64][33]; };
    struct komodo_event_opreturn { uint256 txid; uint64_t value; uint16_t vout,oplen; uint8_t opret[]; };
    struct komodo_event_pricefeed { uint8_t num; uint32_t prices[35]; };

    struct komodo_event
    {
        struct komodo_event *related;
        uint16_t len;
        int32_t height;
        uint8_t type,reorged;
        char symbol[KOMODO_ASSETCHAIN_MAXLEN];
        uint8_t space[];
    };

    /* src/komodo_structs.h */
    class komodo_state
    {
    public:
        int32_t SAVEDHEIGHT;
        int32_t CURRENT_HEIGHT;
        uint32_t SAVEDTIMESTAMP;
        uint64_t deposited;
        uint64_t issued;
        uint64_t withdrawn;
        uint64_t approved;
        uint64_t redeemed;
        uint64_t shorted;
        std::list<std::shared_ptr<komodo::event>> events;
        uint32_t RTbufs[64][3]; uint64_t RTmask;
        bool add_event(const std::string& symbol, const uint32_t height, std::shared_ptr<komodo::event> in);
    protected:
        /***
         * @brief clear the checkpoints collection
         * @note should only be used by tests
         */
        void clear_checkpoints();
        std::vector<notarized_checkpoint> NPOINTS; // collection of notarizations
        mutable size_t NPOINTS_last_index = 0; // caches checkpoint linear search position
        notarized_checkpoint last;

    public:
        const uint256 &LastNotarizedHash() const;
        void SetLastNotarizedHash(const uint256 &in);
        const uint256 &LastNotarizedDestTxId() const;
        void SetLastNotarizedDestTxId(const uint256 &in);
        const uint256 &LastNotarizedMoM() const;
        void SetLastNotarizedMoM(const uint256 &in);
        const int32_t &LastNotarizedHeight() const;
        void SetLastNotarizedHeight(const int32_t in);
        const int32_t &LastNotarizedMoMDepth() const;
        void SetLastNotarizedMoMDepth(const int32_t in);

        /*****
         * @brief add a checkpoint to the collection and update member values
         * @param in the new values
         */
        void AddCheckpoint(const notarized_checkpoint &in);

        uint64_t NumCheckpoints() const;

        /****
         * Get the notarization data below a particular height
         * @param[in] nHeight the height desired
         * @param[out] notarized_hashp the hash of the notarized block
         * @param[out] notarized_desttxidp the desttxid
         * @returns the notarized height
         */
        int32_t NotarizedData(int32_t nHeight,uint256 *notarized_hashp,uint256 *notarized_desttxidp) const;

        /******
         * @brief Get the last notarization information
         * @param[out] prevMoMheightp the MoM height
         * @param[out] hashp the notarized hash
         * @param[out] txidp the DESTTXID
         * @returns the notarized height
         */
        int32_t NotarizedHeight(int32_t *prevMoMheightp,uint256 *hashp,uint256 *txidp);

        /****
         * Search for the last (chronological) MoM notarized height
         * @returns the last notarized height that has a MoM
         */
        int32_t PrevMoMHeight() const;

        /******
         * @brief Search the notarized checkpoints for a particular height
         * @note Finding a mach does include other criteria other than height
         *      such that the checkpoint includes the desired height
         * @param height the notarized_height desired
         * @returns the checkpoint or nullptr
         */
        const notarized_checkpoint *CheckpointAtHeight(int32_t height) const;
    };

    /*****
     * @brief add a checkpoint to the collection and update member values
     * @param in the new values
     */
    void komodo_state::AddCheckpoint(const notarized_checkpoint &in)
    {
        NPOINTS.push_back(in);
        last = in;
    }

    /****
     * Get the notarization data below a particular height
     * @param[in] nHeight the height desired
     * @param[out] notarized_hashp the hash of the notarized block
     * @param[out] notarized_desttxidp the desttxid
     * @returns the notarized height
     */
    int32_t komodo_state::NotarizedData(int32_t nHeight,uint256 *notarized_hashp,uint256 *notarized_desttxidp) const
    {
        bool found = false;

        if ( NPOINTS.size() > 0 )
        {
            const notarized_checkpoint* np = nullptr;
            if ( NPOINTS_last_index < NPOINTS.size() && NPOINTS_last_index > 0 ) // if we cached an NPOINT index
            {
                np = &NPOINTS[NPOINTS_last_index-1]; // grab the previous
                if ( np->nHeight < nHeight ) // if that previous is below the height we are looking for
                {
                    for (size_t i = NPOINTS_last_index; i < NPOINTS.size(); i++) // move forward
                    {
                        if ( NPOINTS[i].nHeight >= nHeight ) // if we found the height we are looking for (or beyond)
                        {
                            found = true; // notify ourselves we were here
                            break; // get out
                        }
                        np = &NPOINTS[i];
                        NPOINTS_last_index = i;
                    }
                }
            }
            if ( !found ) // we still haven't found what we were looking for
            {
                np = nullptr; // reset
                for (size_t i = 0; i < NPOINTS.size(); i++) // linear search from the start
                {
                    if ( NPOINTS[i].nHeight >= nHeight )
                    {
                        found = true;
                        break;
                    }
                    np = &NPOINTS[i];
                    NPOINTS_last_index = i;
                }
            }
            if ( np != nullptr )
            {
                if ( np->nHeight >= nHeight || (found && np[1].nHeight < nHeight) )
                    fprintf(stderr, "warning: flag.%d i.%ld np->ht %d [1].ht %d >= nHeight.%d\n",
                            (int)found,NPOINTS_last_index,np->nHeight,np[1].nHeight,nHeight);
                *notarized_hashp = np->notarized_hash;
                *notarized_desttxidp = np->notarized_desttxid;
                return(np->notarized_height);
            }
        }
        memset(notarized_hashp,0,sizeof(*notarized_hashp));
        memset(notarized_desttxidp,0,sizeof(*notarized_desttxidp));
        return 0;
    }

    /******
     * @brief Get the last notarization information
     * @param[out] prevMoMheightp the MoM height
     * @param[out] hashp the notarized hash
     * @param[out] txidp the DESTTXID
     * @returns the notarized height
     */
    int32_t komodo_state::NotarizedHeight(int32_t *prevMoMheightp,uint256 *hashp,uint256 *txidp)
    {
        /*
        CBlockIndex *pindex;
        if ( (pindex= komodo_blockindex(last.notarized_hash)) == 0 || pindex->nHeight < 0 )
        {
            // found orphaned notarization, adjust the values in the komodo_state object
            last.notarized_hash.SetNull();
            last.notarized_desttxid.SetNull();
            last.notarized_height = 0;
        }
        else
        */
        {
            *hashp = last.notarized_hash;
            *txidp = last.notarized_desttxid;
            *prevMoMheightp = PrevMoMHeight();
        }
        return last.notarized_height;
    }

    /****
     * Search for the last (chronological) MoM notarized height
     * @returns the last notarized height that has a MoM
     */
    int32_t komodo_state::PrevMoMHeight() const
    {
        static uint256 zero;
        // shortcut
        if (last.MoM != zero)
        {
            return last.notarized_height;
        }

        for(auto itr = NPOINTS.rbegin(); itr != NPOINTS.rend(); ++itr)
        {
            if( itr->MoM != zero)
                return itr->notarized_height;
        }
        return 0;
    }

    /******
     * @brief Search the notarized checkpoints for a particular height
     * @note Finding a mach does include other criteria other than height
     *      such that the checkpoint includes the desired height
     * @param height the notarized_height desired
     * @returns the checkpoint or nullptr
     */
    const notarized_checkpoint *komodo_state::CheckpointAtHeight(int32_t height) const
    {
        // find the nearest notarization_height
        // work backwards, get the first one that meets our criteria
        for(auto itr = NPOINTS.rbegin(); itr != NPOINTS.rend(); ++itr)
        {
            if ( itr->MoMdepth != 0
                    && height > itr->notarized_height-(itr->MoMdepth&0xffff) // 2s compliment if negative
                    && height <= itr->notarized_height )
            {
                return &(*itr);
            }
        }
        return nullptr;
    }

    void komodo_state::clear_checkpoints() { NPOINTS.clear(); }
    const uint256& komodo_state::LastNotarizedHash() const { return last.notarized_hash; }
    void komodo_state::SetLastNotarizedHash(const uint256 &in) { last.notarized_hash = in; }
    const uint256& komodo_state::LastNotarizedDestTxId() const { return last.notarized_desttxid; }
    void komodo_state::SetLastNotarizedDestTxId(const uint256 &in) { last.notarized_desttxid = in; }
    const uint256& komodo_state::LastNotarizedMoM() const { return last.MoM; }
    void komodo_state::SetLastNotarizedMoM(const uint256 &in) { last.MoM = in; }
    const int32_t& komodo_state::LastNotarizedHeight() const { return last.notarized_height; }
    void komodo_state::SetLastNotarizedHeight(const int32_t in) { last.notarized_height = in; }
    const int32_t& komodo_state::LastNotarizedMoMDepth() const { return last.MoMdepth; }
    void komodo_state::SetLastNotarizedMoMDepth(const int32_t in) { last.MoMdepth =in; }
    uint64_t komodo_state::NumCheckpoints() const { return NPOINTS.size(); }

    bool operator==(const notarized_checkpoint& lhs, const notarized_checkpoint& rhs)
    {
        if (lhs.notarized_hash != rhs.notarized_hash
                || lhs.notarized_desttxid != rhs.notarized_desttxid
                || lhs.MoM != rhs.MoM
                || lhs.MoMoM != rhs.MoMoM
                || lhs.nHeight != rhs.nHeight
                || lhs.notarized_height != rhs.notarized_height
                || lhs.MoMdepth != rhs.MoMdepth
                || lhs.MoMoMdepth != rhs.MoMoMdepth
                || lhs.MoMoMoffset != rhs.MoMoMoffset
                || lhs.kmdstarti != rhs.kmdstarti
                || lhs.kmdendi != rhs.kmdendi)
            return false;
        return true;
    }


    // komodo_structs.cpp
    bool komodo_state::add_event(const std::string& symbol, const uint32_t height, std::shared_ptr<komodo::event> in)
    {
        // if (ASSETCHAINS_SYMBOL[0] != 0) // DISABLED for debug purposes (!)
        {
            std::lock_guard<std::mutex> lock(komodo_mutex);
            events.push_back( in );
            return true;
        }
        return false;
    }

    // komodo_events.cpp
    void komodo_eventadd_pubkeys(komodo_state *sp, char *symbol, int32_t height, std::shared_ptr<komodo::event_pubkeys> pk)
    {
        if (sp != nullptr)
        {
            sp->add_event(symbol, height, pk);
            /* komodo_notarysinit(height, pk->pubkeys, pk->num); */
        }
    }

    /***
     * Add a notarized checkpoint to the komodo_state
     * @param[in] sp the komodo_state to add to
     * @param[in] nHeight the height
     * @param[in] notarized_height the height of the notarization
     * @param[in] notarized_hash the hash of the notarization
     * @param[in] notarized_desttxid the txid of the notarization on the destination chain
     * @param[in] MoM the MoM
     * @param[in] MoMdepth the depth
     */
    void komodo_notarized_update(struct komodo_state *sp,int32_t nHeight,int32_t notarized_height,
            uint256 notarized_hash,uint256 notarized_desttxid,uint256 MoM,int32_t MoMdepth)
    {
        if ( notarized_height >= nHeight )
        {
            fprintf(stderr,"komodo_notarized_update REJECT notarized_height %d > %d nHeight\n",notarized_height,nHeight);
            return;
        }

        notarized_checkpoint new_cp;
        new_cp.nHeight = nHeight;
        new_cp.notarized_height = notarized_height;
        new_cp.notarized_hash = notarized_hash;
        new_cp.notarized_desttxid = notarized_desttxid;
        new_cp.MoM = MoM;
        new_cp.MoMdepth = MoMdepth;
        std::lock_guard<std::mutex> lock(komodo_mutex);
        sp->AddCheckpoint(new_cp);
    }

    void komodo_eventadd_notarized( komodo_state *sp, char *symbol, int32_t height, std::shared_ptr<komodo::event_notarized> ntz)
    {
        char *coin = (ASSETCHAINS_SYMBOL[0] == 0) ? (char *)"KMD" : ASSETCHAINS_SYMBOL;
        /*
        if ( IS_KOMODO_NOTARY
                && komodo_verifynotarization(symbol,ntz->dest,height,ntz->notarizedheight,ntz->blockhash, ntz->desttxid) < 0 )
        {
            static uint32_t counter;
            if ( counter++ < 100 )
                printf("[%s] error validating notarization ht.%d notarized_height.%d, if on a pruned %s node this can be ignored\n",
                        ASSETCHAINS_SYMBOL,height,ntz->notarizedheight, ntz->dest);
        }
        else
        */
        if ( strcmp(symbol,coin) == 0 )
        {
            if ( sp != nullptr )
            {
                sp->add_event(symbol, height, ntz);
                komodo_notarized_update(sp,height, ntz->notarizedheight, ntz->blockhash, ntz->desttxid, ntz->MoM, ntz->MoMdepth);
            }
        }
    }

    void komodo_setkmdheight(struct komodo_state *sp,int32_t kmdheight,uint32_t timestamp)
    {
        if ( sp != nullptr )
        {
            if ( kmdheight > sp->SAVEDHEIGHT )
            {
                sp->SAVEDHEIGHT = kmdheight;
                sp->SAVEDTIMESTAMP = timestamp;
            }
            if ( kmdheight > sp->CURRENT_HEIGHT )
                sp->CURRENT_HEIGHT = kmdheight;
        }
    }

    void komodo_event_undo(komodo_state *sp, std::shared_ptr<komodo::event> ev)
    {
        switch ( ev->type )
        {
            case KOMODO_EVENT_RATIFY:
                printf("rewind of ratify, needs to be coded.%d\n",ev->height);
                break;
            case KOMODO_EVENT_NOTARIZED:
                break;
            case KOMODO_EVENT_KMDHEIGHT:
                if ( ev->height <= sp->SAVEDHEIGHT )
                    sp->SAVEDHEIGHT = ev->height;
                break;
            case KOMODO_EVENT_PRICEFEED:
                // backtrack prices;
            case KOMODO_EVENT_OPRETURN:
                    // backtrack opreturns
                    break;
        }
    }

    void komodo_event_rewind(struct komodo_state *sp,char *symbol,int32_t height)
    {
        komodo_event *ep = nullptr;
        if ( sp != nullptr )
        {
            if ( ASSETCHAINS_SYMBOL[0] == 0 && height <= KOMODO_LASTMINED && prevKOMODO_LASTMINED != 0 )
            {
                printf("undo KOMODO_LASTMINED %d <- %d\n",KOMODO_LASTMINED,prevKOMODO_LASTMINED);
                KOMODO_LASTMINED = prevKOMODO_LASTMINED;
                prevKOMODO_LASTMINED = 0;
            }
            rewind_count++;

        while ( sp->events.size() > 0)
            {
                auto ev = sp->events.back(); // read last (!) element sp->Komodo_events[sp->Komodo_numevents-1]
                if (ev-> height < height)
                    break;
                komodo_event_undo(sp, ev);
                sp->events.pop_back();
            }
        }
    }

    const char *komodo_opreturn(int32_t height,uint64_t value,uint8_t *opretbuf,int32_t opretlen,uint256 txid,uint16_t vout,char *source)
    {
        const char *typestr = "unknown";
        return typestr;
    }

    void komodo_eventadd_opreturn( komodo_state *sp, char *symbol, int32_t height, std::shared_ptr<komodo::event_opreturn> opret)
    {
        if ( sp != nullptr && ASSETCHAINS_SYMBOL[0] != 0)
        {
            sp->add_event(symbol, height, opret);
            komodo_opreturn(height, opret->value, opret->opret.data(), opret->opret.size(), opret->txid, opret->vout, symbol);
        }
    }

    void komodo_eventadd_pricefeed( komodo_state *sp, char *symbol, int32_t height, std::shared_ptr<komodo::event_pricefeed> pf)
    {
        if (sp != nullptr)
        {
            sp->add_event(symbol, height, pf);
            /* komodo_pvals(height,pf->prices, pf->num); */
        }
    }

    void komodo_eventadd_kmdheight(struct komodo_state *sp,char *symbol,int32_t height, std::shared_ptr<komodo::event_kmdheight> kmdht)
    {
        if (sp != nullptr)
        {
            // static int32_t komodo_eventadd_kmdheight_call_count;
            // std::cerr << __func__ << " [" << sp->events.size() <<"] " << ++komodo_eventadd_kmdheight_call_count << " " << height << " " << kmdht->kheight << " " << kmdht->timestamp << std::endl;

            if ( kmdht->kheight > 0 ) // height is advancing
            {

                sp->add_event(symbol, height, kmdht);
                komodo_setkmdheight(sp, kmdht->kheight, kmdht->timestamp);
            }
            else // rewinding
            {
                std::shared_ptr<komodo::event_rewind> e = std::make_shared<komodo::event_rewind>(height);
                sp->add_event(symbol, height, e);
                komodo_event_rewind(sp,symbol,height);
            }
        }
    }

    int32_t komodo_parsestatefile(struct komodo_state *sp,FILE *fp,char *symbol,char *dest)
    {
        int32_t func;

        try
        {
            if ( (func= fgetc(fp)) != EOF )
            {
                bool matched = false;
                if ( ASSETCHAINS_SYMBOL[0] == 0 && strcmp(symbol,"KMD") == 0 )
                    matched = true;
                else
                    matched = (strcmp(symbol,ASSETCHAINS_SYMBOL) == 0);

                int32_t ht;
                if ( fread(&ht,1,sizeof(ht),fp) != sizeof(ht) )
                    throw komodo::parse_error("Unable to read height from file");

                // if (ht == 2441413) {
                //     std::cerr << "[new] Here we should break ... " << std::endl;
                // }

                if ( func == 'P' )
                {
                    std::shared_ptr<komodo::event_pubkeys> pk = std::make_shared<komodo::event_pubkeys>(fp, ht);
                    if ( (KOMODO_EXTERNAL_NOTARIES && matched ) || (strcmp(symbol,"KMD") == 0 && !KOMODO_EXTERNAL_NOTARIES) )
                    {
                        komodo_eventadd_pubkeys(sp, symbol, ht, pk);
                    }
                }
                else if ( func == 'N' || func == 'M' )
                {
                    std::shared_ptr<komodo::event_notarized> evt = std::make_shared<komodo::event_notarized>(fp, ht, dest, func == 'M');
                    komodo_eventadd_notarized(sp, symbol, ht, evt);
                }
                else if ( func == 'U' ) // deprecated
                {
                    std::shared_ptr<komodo::event_u> evt = std::make_shared<komodo::event_u>(fp, ht);
                }
                else if ( func == 'K' || func == 'T')
                {
                    std::shared_ptr<komodo::event_kmdheight> evt = std::make_shared<komodo::event_kmdheight>(fp, ht, func == 'T');
                    komodo_eventadd_kmdheight(sp, symbol, ht, evt);
                }
                else if ( func == 'R' )
                {
                    std::shared_ptr<komodo::event_opreturn> evt = std::make_shared<komodo::event_opreturn>(fp, ht);
                    // check for oversized opret
                    if ( evt->opret.size() < 16384*4 )
                        komodo_eventadd_opreturn(sp, symbol, ht, evt);
                }
                else if ( func == 'D' )
                {
                    printf("unexpected function D[%d]\n",ht);
                }
                else if ( func == 'V' )
                {
                    std::shared_ptr<komodo::event_pricefeed> evt = std::make_shared<komodo::event_pricefeed>(fp, ht);
                    komodo_eventadd_pricefeed(sp, symbol, ht, evt);
                }
            } // retrieved the func
        }
        catch(const komodo::parse_error& pe)
        {
            //LogPrintf("Error occurred in parsestatefile: %s\n", pe.what());
            std::cerr << strprintf("Error occurred in parsestatefile: %s\n", pe.what()) << std::endl;
            func = -1;
        }
        return func;
    }

}

/***
 * @brief persist event to file stream
 * @param evt the event
 * @param fp the file
 * @returns the number of bytes written
 */
size_t write_event_mistake(std::shared_ptr<komodo::event> evt, FILE *fp)
{
    std::stringstream ss;
    ss << evt;
    std::string buf = ss.str();
    return fwrite(buf.c_str(), buf.size(), 1, fp);
}

/***
 * @brief persist event to file stream
 * @param evt the event
 * @param fp the file
 * @returns the number of bytes written
 */
template<class T>
size_t write_event(T& evt, FILE *fp)
{
    std::stringstream ss;
    ss << evt;
    std::string buf = ss.str();
    return fwrite(buf.c_str(), buf.size(), 1, fp);
}

bool operator==(const komodo::event_pubkeys &lhs, const komodo::event_pubkeys &rhs)
{
    if (lhs.height != rhs.height ||
        lhs.num != rhs.num ||
        lhs.type != rhs.type)
        return false;
    const size_t maxsz = sizeof(komodo::event_pubkeys::pubkeys) / sizeof(komodo::event_pubkeys::pubkeys[0]);
    for (int i = 0; i < std::min(static_cast<size_t>(lhs.num), maxsz); i++)
    {
        if (memcmp(&lhs.pubkeys[i][0], &rhs.pubkeys[i][0], 33) != 0)
            return false;
    }
    return true;
}

int main() {

    std::cout << "--- Komodo Events :: Simple Tests (q) Decker ---" << std::endl;
    // events_old::f();
    // uint256 a;
    // for (int i = 0; i<32; i++) a.bytes[i] = 0; a.uints[0] = 0xdeadc0de;
    // std::cout << a.ToString() << std::endl;

    // komodo_faststateinit -> komodo_parsestatefiledata
    // komodo_passport_iteration -> komodo_parsestatefile

    FILE *fp; uint8_t *filedata; long fpos,datalen,lastfpos;

    struct events_old::komodo_state KOMODO_STATE_OLD;
    memset(&KOMODO_STATE_OLD, 0, sizeof(KOMODO_STATE_OLD));
    struct events_old::komodo_state *sp_old = &KOMODO_STATE_OLD;

    struct events_new::komodo_state KOMODO_STATE_NEW;
    // memset(&KOMODO_STATE_NEW, 0, sizeof(KOMODO_STATE_NEW)); // we can't do such thing here, bcz we will damage std::list

    // let's init important values (TODO: check that in original struct in daemon code we doing the same)
    KOMODO_STATE_NEW.SAVEDHEIGHT = 0;
    KOMODO_STATE_NEW.CURRENT_HEIGHT = 0;
    KOMODO_STATE_NEW.SetLastNotarizedHeight(0);
    KOMODO_STATE_NEW.SAVEDTIMESTAMP = 0;

    struct events_new::komodo_state *sp_new = &KOMODO_STATE_NEW;

    fp= fopen("komodostate","rb");
    if (fp) {
        fseek(fp,0,SEEK_END);
        fprintf(stderr,"%s: reading %ldKB\n","komodostate",ftell(fp)/1024);

        const char *symbol = "KMD";
        const char *dest = "LTC";
        // const char *symbol = "MCL";
        // const char *dest = "KMD";

        // old events processing
        int32_t read_count = 0;
        fseek(fp, 0, SEEK_SET);
        while ( events_old::komodo_parsestatefile(sp_old,fp,(char *)symbol,(char *)dest) >= 0 )
        {
            read_count++;
        }

        std::cerr << std::endl;
        std::cerr << "read_count = " << read_count << std::endl;
        std::cerr << "rewind_count = " << events_old::rewind_count << std::endl;
        std::cerr << "sp->NUM_NPOINTS = " << sp_old->NUM_NPOINTS << std::endl;
        std::cerr << "sp->Komodo_numevents = " << sp_old->Komodo_numevents << std::endl;


        /* for (size_t i = 0; i < sp_old->Komodo_numevents; i++) {
            struct events_old::komodo_event *p_event = sp_old->Komodo_events[i];
            if (!(p_event->type == KOMODO_EVENT_KMDHEIGHT || p_event->type == KOMODO_EVENT_NOTARIZED))
                std::cerr << i << ". " << p_event->type << std::endl;
        } */

        std::cerr << std::endl;
        // new events processing
        read_count = 0;
        fseek(fp, 0, SEEK_SET);
        while ( events_new::komodo_parsestatefile(sp_new,fp,(char *)symbol,(char *)dest) >= 0 )
        {
            read_count++;
        }

        std::cerr << "read_count = " << read_count << std::endl;
        std::cerr << "rewind_count = " << events_new::rewind_count << std::endl;
        std::cerr << "sp->NUM_NPOINTS = " << sp_new->NumCheckpoints() << std::endl;
        // std::cerr << "sp->Komodo_numevents = " << sp_new->Komodo_numevents << std::endl;
        std::cerr << "sp->events.size() = " << sp_new->events.size() << std::endl;

        fclose(fp);
    }

    const char* komodo_event_type_names[] = {
            "EVENT_PUBKEYS",
            "N", // "EVENT_NOTARIZED",
            "EVENT_U",
            "K", // "EVENT_KMDHEIGHT",
            "EVENT_OPRETURN",
            "EVENT_PRICEFEED",
            "EVENT_REWIND"
    };

    assert(sp_old->Komodo_numevents == sp_new->events.size());

    size_t i = 0;
    for (const std::shared_ptr<komodo::event>& ptr : sp_new->events) {
        struct events_old::komodo_event *p_event = sp_old->Komodo_events[i];
        assert(p_event->height == ptr->height);
        i++;
    }

    /* for (size_t i = 0; i < sp_old->Komodo_numevents; i++) {
            struct events_old::komodo_event *p_event = sp_old->Komodo_events[i];
            std::cerr << i << ". " << p_event->type << " " << p_event->height << std::endl;
    }

    size_t i = 0;
    for (const std::shared_ptr<komodo::event>& ptr : sp_new->events) {
        std::cerr << i++ << ". " << komodo_event_type_names[ptr->type] << " " << ptr->height << std::endl;
    } */

    std::cerr << sp_old->NOTARIZED_HASH.ToStringRev() << " - " << sp_new->LastNotarizedHash().ToStringRev() << std::endl;
    assert(sp_old->NOTARIZED_HASH.ToStringRev() == sp_new->LastNotarizedHash().ToStringRev());
    std::cerr << sp_old->NOTARIZED_DESTTXID.ToStringRev() << " - " << sp_new->LastNotarizedDestTxId().ToStringRev() << std::endl;
    assert(sp_old->NOTARIZED_DESTTXID.ToStringRev() == sp_new->LastNotarizedDestTxId().ToStringRev());
    std::cerr << sp_old->MoM.ToStringRev() << " - " << sp_new->LastNotarizedMoM().ToStringRev() << std::endl;
    assert(sp_old->MoM.ToStringRev() == sp_new->LastNotarizedMoM().ToStringRev());
    std::cerr << sp_old->SAVEDHEIGHT << " - " << sp_new->SAVEDHEIGHT << std::endl;
    assert(sp_old->SAVEDHEIGHT == sp_new->SAVEDHEIGHT);
    std::cerr << sp_old->CURRENT_HEIGHT << " - " << sp_new->CURRENT_HEIGHT << std::endl;
    assert(sp_old->CURRENT_HEIGHT == sp_new->CURRENT_HEIGHT);
    std::cerr << sp_old->NOTARIZED_HEIGHT << " - " << sp_new->LastNotarizedHeight() << std::endl;
    assert(sp_old->NOTARIZED_HEIGHT == sp_new->LastNotarizedHeight());
    std::cerr << sp_old->SAVEDTIMESTAMP << " - " << sp_new->SAVEDTIMESTAMP << std::endl;
    assert(sp_old->SAVEDTIMESTAMP == sp_new->SAVEDTIMESTAMP);
    std::cerr << sp_old->NUM_NPOINTS << " - " << sp_new->NumCheckpoints() << std::endl;
    assert(sp_old->NUM_NPOINTS == sp_new->NumCheckpoints());
    // TODO: compare NPOINTS (!)

    /* TODO: Events create, serializing, write to file tests ... */

    /* event_pubkeys tests #1 */
    {
        fp = fopen("events.bin", "wb");
        if (fp)
        {
            fseek(fp, 0, SEEK_END);

            std::shared_ptr<komodo::event_pubkeys> evt = std::make_shared<komodo::event_pubkeys>(1);
            evt->num = 64;
            for (size_t i = 0; i < 64; i++)
            {
                memset(&evt->pubkeys[i], i, 33);
            }

            /* error: write a pointer, like 0x10100a218, instead of object content */
            // write_event(evt, fp);
            write_event(*evt, fp);

            // https://en.cppreference.com/w/c/language/array_initialization
            // https://stackoverflow.com/questions/38892455/initializing-an-array-of-zeroes

            uint8_t data[1 + 64 * 33] = {1}; // first byte is size, and other 64 * 33 are pubkeys 0..63

            for (size_t i = 0; i < 64; i++)
            {
                memset(1 + &data[0] + i * 33, i, 33);
            }

            long pos;
            pos = 0;
            komodo::event_pubkeys kep(&data[0], pos, sizeof(data), 1);
            pos = 0; // don't forget to zero pos in data from which data will be read
            std::shared_ptr<komodo::event_pubkeys> evt_data = std::make_shared<komodo::event_pubkeys>(&data[0], pos, sizeof(data), 1);

            bool fEqual = *evt_data == kep;
            std::cout << "fEqual = " << (fEqual ? "true" : "false") << std::endl;
            // https://github.com/KomodoPlatform/komodo/pull/510 - reference to a bug with uninitialized pubkeys member.
            assert(fEqual);

            fclose(fp);
        }
    }

    /* event_pubkeys tests #2 */
    {
        fp = fopen("events.bin","w+b");
        if (fp) {
            uint8_t data[1 + 64 * 33] = { 1 };
            for (size_t i = 0; i < 64; i++)
                memset(1 + &data[0] + i * 33, i, 33);
            fwrite(data, sizeof(data), 1, fp);
            fseek(fp, 0, SEEK_SET);
            komodo::event_pubkeys kep1(fp, 777);
            long pos = 0;
            komodo::event_pubkeys kep2(data, pos, sizeof(data), 777);
            fclose(fp);
            assert(kep1 == kep2);
        }
    }
    // here #510 bug exists as well, if num of read records < 64 (records with indexes higher than num remains unitialized)

    /* event_notarized tests #1 */
    {
        komodo::event_notarized ken1(0x04030201, "KMD");
        std::string ser = HexStr((std::stringstream() << ken1).str());
        std::cout << "ken1: '" << ser << "'" << std::endl;
        assert(ser == "4e010203040000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000");
        assert(strlen(ken1.dest) == 3);
        assert(ken1.height == 0x04030201);

        long pos = 0;
        uint8_t data[] = {
            0xc2, 0x7f, 0x2e, 0x00, // notarizedheight = #3047362
            0x08,0x94,0x3e,0x32,0x34,0x22,0xa1,0xf8,0x5a,0x7d,0x52,0x46,0x74,0x96,0x7c,0x0a,0x25,0x71,0xd0,0xb9,0xfa,0x03,0x57,0xe4,0x2b,0x7f,0x83,0x97,0xd7,0xcf,0x46,0xb9, // blockhash
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // desttxid
        };

        komodo::event_notarized ken2(&data[0], pos, sizeof(data), 0x04030201, "KMD", false);
        ser = HexStr((std::stringstream() << ken2).str());
        std::cout << "ken2: '" << ser << "'" << std::endl;
        assert(ser == "4e01020304c27f2e0008943e323422a1f85a7d524674967c0a2571d0b9fa0357e42b7f8397d7cf46b90000000000000000000000000000000000000000000000000000000000000000");
        assert(ser.size() == 73 * 2);
        assert(strlen(ken2.dest) == 3);
        assert(ken2.height == 0x04030201);

        // this serialization actually using in write_event
        FILE *fp;
        fp = fopen("events.bin", "w+b");
        if (fp) {
            std::shared_ptr<komodo::event_notarized> p_ken3 = std::make_shared<komodo::event_notarized>(ken2);
            // write_event_mistake(p_ken3, fp);
            write_event(*p_ken3, fp);

            // totally broken :(( all komodostate records are pointers representation in ASCII, instead of real data
            fclose(fp);
        }
    }

    /* small serialization test */
    {
        int32_t a = 0x33323130;
        std::cout << "a: '" << komodo::serializable<int32_t>(a) << "' (" << HexStr((std::stringstream() << komodo::serializable<int32_t>(a)).str()) << ")" << std::endl;
        assert(HexStr((std::stringstream() << komodo::serializable<int32_t>(a)).str()) == "30313233");
    }

    /* writing all type of events to a separate file */
    {
        std::cout << std::endl;
        int32_t height = 0x01020304; // 16909060
        std::string ser;

        FILE *fp;
        fp = fopen("allevents.bin","w+b");
        if (fp) {
            /* EVENT_PUBKEYS */
            komodo::event_pubkeys evt_pubkeys(height);
            evt_pubkeys.num = 1;
            for(int i = 0; i < 33; ++i) evt_pubkeys.pubkeys[0][i] = i;

            ser.clear();
            ser = HexStr((std::stringstream() << evt_pubkeys).str());
            std::cout << "EVENT_PUBKEYS: " << ser << std::endl;
            write_event(evt_pubkeys, fp);

            /* EVENT_NOTARIZED */
            komodo::event_notarized evt_notarized(height, "KMD");
            ser.clear();
            evt_notarized.notarizedheight = 0x5;
            for (int i = 0; i < 32; ++i) evt_notarized.blockhash.bytes[i] = 0x11;
            for (int i = 0; i < 32; ++i) evt_notarized.desttxid.bytes[i] = 0x22;
            for (int i = 0; i < 32; ++i) evt_notarized.MoM.bytes[i] = 0x33;
            evt_notarized.MoMdepth = 0x4;

            ser = HexStr((std::stringstream() << evt_notarized).str());
            std::cout << "EVENT_NOTARIZED: " << ser << std::endl;
            write_event(evt_notarized, fp);

            /* EVENT_U */
            komodo::event_u evt_u(height);
            ser.clear();
            evt_u.n = 0xFF;
            evt_u.nid = 0XFE;
            for (int i=0; i<8; ++i) evt_u.mask[i] = 0x40 + i;
            for (int i=0; i<32; ++i) evt_u.hash[i] = 0x50 + i;

            ser = HexStr((std::stringstream() << evt_u).str());
            std::cout << "EVENT_U: " << ser << std::endl;
            write_event(evt_u, fp);

            /* EVENT_KMDHEIGHT */
            /* EVENT_OPRETURN */
            /* EVENT_PRICEFEED */
            /* EVENT_REWIND */
            fclose(fp);
        }
    }
    return 0;
}
