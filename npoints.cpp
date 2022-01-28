#include <iostream>
#include <cstdint>
#include <cstring>
#include <stdio.h>
#include "tinyformat.h"
#include "colors.h"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>

#include <vector>
#include <chrono>

#define BITS 256

/* utilstrencodings.cpp */
const signed char p_util_hexdigit[256] =
{ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  0,1,2,3,4,5,6,7,8,9,-1,-1,-1,-1,-1,-1,
  -1,0xa,0xb,0xc,0xd,0xe,0xf,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,0xa,0xb,0xc,0xd,0xe,0xf,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, };

signed char HexDigit(char c)
{
    return p_util_hexdigit[(unsigned char)c];
}

union _bits256 {

    uint8_t bytes[32]; uint16_t ushorts[16]; uint32_t uints[8]; uint64_t ulongs[4]; uint64_t txid;

    enum { WIDTH=BITS/8 };
    std::string ToString()
    {
        std::string res = "";
        for (int i=0; i<32; i++)
            res = res + strprintf("%02x", bytes[i]);
        return res;
    }
    std::string ToStringRev()
    {
        std::string res = "";
        for (int i=0; i<32; i++)
            res = strprintf("%02x", bytes[i]) + res;
        return res;
    }
    void SetNull() {
        for (int i=0; i<32; i++) bytes[i] = 0;
    }

    void SetHex(const char* psz)
    {
        memset(bytes, 0, sizeof(bytes));

        // skip leading spaces
        while (isspace(*psz))
            psz++;

        // skip 0x
        if (psz[0] == '0' && tolower(psz[1]) == 'x')
            psz += 2;

        // hex string to uint
        const char* pbegin = psz;
        while (::HexDigit(*psz) != -1)
            psz++;
        psz--;
        unsigned char* p1 = (unsigned char*)bytes;
        unsigned char* pend = p1 + WIDTH;
        while (psz >= pbegin && p1 < pend) {
            *p1 = ::HexDigit(*psz--);
            if (psz >= pbegin) {
                *p1 |= ((unsigned char)::HexDigit(*psz--) << 4);
                p1++;
            }
        }
    }

    void SetHex(const std::string &s) {
        SetHex(s.c_str());
    }

    _bits256() {
        SetNull();
    }

    explicit _bits256(const std::string _bytes) {
        SetHex(_bytes);
    }

    _bits256& operator= (const std::string _bytes) {
        SetHex(_bytes);
        return *this;
    }

    _bits256(const char *psz) {
        SetHex(psz);
    }

    bool operator != (const _bits256 &other) const {
        bool fEqual = true;
        for (size_t i=0; i<32; i++)
            if (this->bytes[i] != other.bytes[i])
                { fEqual = false; break; }
        return !fEqual;
    }

    bool operator == (const _bits256 &other) const {
        return !(*this != other);
    }

    friend std::ostream& operator<<(std::ostream& out, const _bits256 &b);
};

std::ostream& operator<<(std::ostream& out, const _bits256 &b) {
    std::string res = "";
    bool fIsNull = true;
    for (int i=0; i<32; i++) if (b.bytes[i]) { fIsNull=false; break; }

    if (!fIsNull) {
        for (int i=0; i<32; i++)
            res = strprintf("%02x", b.bytes[i]) + res;
    } else
            res = "0";

    out << res;
    return out;
}

typedef union _bits256 bits256;
typedef union _bits256 uint256;

struct notarized_checkpoint /* komodo_structs.h */
    {
        uint256 notarized_hash,notarized_desttxid,MoM,MoMoM;
        int32_t nHeight,notarized_height,MoMdepth,MoMoMdepth,MoMoMoffset,kmdstarti,kmdendi;
        friend std::ostream& operator<<(std::ostream& out, const notarized_checkpoint &nc);
        bool operator == (const struct notarized_checkpoint  &other) const {
            return (this->notarized_hash == other.notarized_hash &&
                    this->notarized_desttxid == other.notarized_desttxid &&
                    this->MoM == other.MoM &&
                    this->MoMoM == other.MoMoM &&
                    this->nHeight == other.nHeight &&
                    this->notarized_height == other.notarized_height &&
                    this->MoMdepth == other.MoMdepth &&
                    this->MoMoMdepth == other.MoMoMdepth);
        }
        bool operator != (const struct notarized_checkpoint  &other) const {
            return !(*this == other);
        }
    };

std::ostream& operator<<(std::ostream& out, const notarized_checkpoint &nc) {
    out << "{"
        << "\"0x" << nc.notarized_hash << "\"" << ", "
        << "\"0x" << nc.notarized_desttxid << "\"" << ", "
        << "\"0x" << nc.MoM << "\"" << ", "
        << "\"0x" << nc.MoMoM << "\"" << ", "
        << nc.nHeight << ", " << nc.notarized_height << ", " << nc.MoMdepth << ", "
        << nc.MoMoMdepth << ", "
        << nc.MoMoMoffset << ", " << nc.kmdstarti << ", " << nc.kmdendi
        << "}";
    return out;
}
typedef struct notarized_checkpoint t_notarized_checkpoint;

#include "npoints-data.h"

#define portable_mutex_lock pthread_mutex_lock
#define portable_mutex_unlock pthread_mutex_unlock
#define KOMODO_ASSETCHAIN_MAXLEN 65 /* util.h, bitcoind.cpp, komodo_defs.h, komodo_globals.h, komodo_structs.h */
char ASSETCHAINS_SYMBOL[] = {0};
// char ASSETCHAINS_SYMBOL[] = {'M','C','L', 0}

/* typedef boost::multi_index::multi_index_container<
        notarized_checkpoint,
        boost::multi_index::indexed_by<
                boost::multi_index::sequenced<>, // sorted by insertion order
                boost::multi_index::ordered_non_unique<
                        boost::multi_index::member<
                                notarized_checkpoint, int32_t, &notarized_checkpoint::nHeight
                        >
                >, // sorted by nHeight
                boost::multi_index::ordered_non_unique<
                        boost::multi_index::member<
                                notarized_checkpoint, int32_t, &notarized_checkpoint::notarized_height
                        >
                > // sorted by notarized_height
        > > notarized_checkpoint_container; */

namespace old_space {

    struct komodo_state
    {
        uint256 NOTARIZED_HASH,NOTARIZED_DESTTXID,MoM;
        int32_t SAVEDHEIGHT,CURRENT_HEIGHT,NOTARIZED_HEIGHT,MoMdepth;
        uint32_t SAVEDTIMESTAMP;
        uint64_t deposited,issued,withdrawn,approved,redeemed,shorted;
        struct notarized_checkpoint *NPOINTS; int32_t NUM_NPOINTS,last_NPOINTSi;
        //std::list<std::shared_ptr<komodo::event>> events;
        uint32_t RTbufs[64][3]; uint64_t RTmask;
        //bool add_event(const std::string& symbol, const uint32_t height, std::shared_ptr<komodo::event> in);
    };

    struct komodo_state ks_old;
    struct komodo_state *sp = &ks_old;
    pthread_mutex_t komodo_mutex,staked_mutex;

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
    struct notarized_checkpoint *komodo_npptr_for_height(int32_t height, int *idx)
    {
        char symbol[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN]; int32_t i; struct komodo_state *sp; struct notarized_checkpoint *np = 0;
        //if ( (sp= komodo_stateptr(symbol,dest)) != 0 )
        sp = &old_space::ks_old;

        {
            for (i=sp->NUM_NPOINTS-1; i>=0; i--)
            {
                *idx = i;
                np = &sp->NPOINTS[i];
                if ( np->MoMdepth != 0 && height > np->notarized_height-(np->MoMdepth&0xffff) && height <= np->notarized_height )
                    return(np);
            }
        }
        *idx = -1;
        return(0);
    }

    struct notarized_checkpoint *komodo_npptr(int32_t height)
    {
        int idx;
        return komodo_npptr_for_height(height, &idx);
    }

    struct notarized_checkpoint *komodo_npptr_at(int idx)
    {
        char symbol[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN]; struct komodo_state *sp;
        //if ( (sp= komodo_stateptr(symbol,dest)) != 0 )
        sp = &old_space::ks_old;

            if (idx < sp->NUM_NPOINTS)
                return &sp->NPOINTS[idx];
        return(0);
    }

    int32_t komodo_prevMoMheight()
    {
        static uint256 zero;
        zero.SetNull();

        char symbol[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN]; int32_t i; struct komodo_state *sp; struct notarized_checkpoint *np = 0;
        //if ( (sp= komodo_stateptr(symbol,dest)) != 0 )
        sp = &old_space::ks_old;
        {
            for (i=sp->NUM_NPOINTS-1; i>=0; i--)
            {
                np = &sp->NPOINTS[i];
                if ( np->MoM != zero )
                    return(np->notarized_height);
            }
        }
        return(0);
    }

    int32_t komodo_notarizeddata(int32_t nHeight,uint256 *notarized_hashp,uint256 *notarized_desttxidp)
    {
        struct notarized_checkpoint *np = 0;
        int32_t i=0,flag = 0;
        char symbol[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN];
        struct komodo_state *sp;

        //if ( (sp= komodo_stateptr(symbol,dest)) != 0 )
        sp = &old_space::ks_old;
        {
            if ( sp->NUM_NPOINTS > 0 )
            {
                flag = 0;
                if ( sp->last_NPOINTSi < sp->NUM_NPOINTS && sp->last_NPOINTSi > 0 )
                {
                    np = &sp->NPOINTS[sp->last_NPOINTSi-1];
                    if ( np->nHeight < nHeight )
                    {
                        for (i=sp->last_NPOINTSi; i<sp->NUM_NPOINTS; i++)
                        {
                            if ( sp->NPOINTS[i].nHeight >= nHeight )
                            {
                                //printf("flag.1 i.%d np->ht %d [%d].ht %d >= nHeight.%d, last.%d num.%d\n",i,np->nHeight,i,sp->NPOINTS[i].nHeight,nHeight,sp->last_NPOINTSi,sp->NUM_NPOINTS);
                                flag = 1;
                                break;
                            }
                            np = &sp->NPOINTS[i];
                            sp->last_NPOINTSi = i;
                        }
                    }
                }
                if ( flag == 0 )
                {
                    np = 0;
                    for (i=0; i<sp->NUM_NPOINTS; i++)
                    {
                        if ( sp->NPOINTS[i].nHeight >= nHeight )
                        {
                            //printf("i.%d np->ht %d [%d].ht %d >= nHeight.%d\n",i,np->nHeight,i,sp->NPOINTS[i].nHeight,nHeight);
                            break;
                        }
                        np = &sp->NPOINTS[i];
                        sp->last_NPOINTSi = i;
                    }
                }
            }
            if ( np != 0 )
            {
                //char str[65],str2[65]; printf("[%s] notarized_ht.%d\n",ASSETCHAINS_SYMBOL,np->notarized_height);
                if ( np->nHeight >= nHeight || (i < sp->NUM_NPOINTS && np[1].nHeight < nHeight) )
                    printf("warning: flag.%d i.%d np->ht %d [1].ht %d >= nHeight.%d\n",flag,i,np->nHeight,np[1].nHeight,nHeight);
                *notarized_hashp = np->notarized_hash;
                *notarized_desttxidp = np->notarized_desttxid;
                return(np->notarized_height);
            }
        }
        memset(notarized_hashp,0,sizeof(*notarized_hashp));
        memset(notarized_desttxidp,0,sizeof(*notarized_desttxidp));
        return(0);
    }

};

namespace new_space {

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
            struct komodo_event **Komodo_events; int32_t Komodo_numevents;
            uint32_t RTbufs[64][3]; uint64_t RTmask;

        protected:
            void clear_checkpoints(); // for tests only
            std::vector<notarized_checkpoint> NPOINTS; // collection of notarizations
            mutable size_t NPOINTS_last_index = 0; // caches checkpoint linear search position
            notarized_checkpoint last;

        public:
            const uint256 &LastNotarizedHash() const { return last.notarized_hash; }
            void SetLastNotarizedHash(const uint256 &in) { last.notarized_hash = in; }
            const uint256 &LastNotarizedDestTxId() const { return last.notarized_desttxid; }
            void SetLastNotarizedDestTxId(const uint256 &in) { last.notarized_desttxid = in; }
            const uint256 &LastNotarizedMoM() const { return last.MoM; }
            void SetLastNotarizedMoM(const uint256 &in) { last.MoM = in; }
            const int32_t &LastNotarizedHeight() const { return last.notarized_height; }
            void SetLastNotarizedHeight(const int32_t in) { last.notarized_height = in; }
            const int32_t &LastNotarizedMoMDepth() const { return last.MoMdepth; }
            void SetLastNotarizedMoMDepth(const int32_t in) { last.MoMdepth =in; }

            /*****
             * @brief add a checkpoint to the collection and update member values
             * @param in the new values
             */
            void AddCheckpoint(const notarized_checkpoint &in)
            {
                NPOINTS.push_back(in);
                last = in;
            }

            uint64_t NumCheckpoints() const { return NPOINTS.size(); }

            /****
             * Get the notarization data below a particular height
             * @param[in] nHeight the height desired
             * @param[out] notarized_hashp the hash of the notarized block
             * @param[out] notarized_desttxidp the desttxid
             * @returns the notarized height
             */
            int32_t NotarizedData(int32_t nHeight,uint256 *notarized_hashp,uint256 *notarized_desttxidp) const
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
                            printf("warning: flag.%d i.%ld np->ht %d [1].ht %d >= nHeight.%d\n",
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
            int32_t NotarizedHeight(int32_t *prevMoMheightp,uint256 *hashp,uint256 *txidp)
            {
                /* CBlockIndex *pindex;
                if ( (pindex= komodo_blockindex(last.notarized_hash)) == 0 || pindex->GetHeight() < 0 )
                {
                    // found orphaned notarization, adjust the values in the komodo_state object
                    last.notarized_hash.SetNull();
                    last.notarized_desttxid.SetNull();
                    last.notarized_height = 0;
                }
                else */

                // for test purposes we will count that the nota is not orphaned, i.e. all checks above passed
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
            int32_t PrevMoMHeight() const
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
            const notarized_checkpoint *CheckpointAtHeight(int32_t height) const
            {
                // find the nearest notarization_height
                // work backwards, get the first one that meets our criteria
                auto itr = NPOINTS.rbegin();
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
    };

    //std::shared_ptr<komodo_state> sp(new komodo_state());
    komodo_state ks_new;
    komodo_state *sp = &ks_new;

    pthread_mutex_t komodo_mutex,staked_mutex;

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
        memset(&new_cp,0,sizeof(new_cp));
        new_cp.nHeight = nHeight;
        new_cp.notarized_height = notarized_height;
        new_cp.notarized_hash = notarized_hash;
        new_cp.notarized_desttxid = notarized_desttxid;
        new_cp.MoM = MoM;
        new_cp.MoMdepth = MoMdepth;
        portable_mutex_lock(&komodo_mutex);
        sp->AddCheckpoint(new_cp);
        portable_mutex_unlock(&komodo_mutex);
    }

    /******
     * @brief Search the notarized checkpoints for a particular height
     * @note Finding a mach does include other criteria other than height
     *      such that the checkpoint includes the desired hight
     * @param height the key
     * @returns the checkpoint or nullptr
     */
    const notarized_checkpoint *komodo_npptr(int32_t height)
    {
        char symbol[KOMODO_ASSETCHAIN_MAXLEN];
        char dest[KOMODO_ASSETCHAIN_MAXLEN];
        komodo_state *sp;

        //if ( (sp= komodo_stateptr(symbol,dest)) != 0 )
        sp = &new_space::ks_new;
        {
            return sp->CheckpointAtHeight(height);
        }
        return nullptr;
    }

    /****
     * Search for the last (chronological) MoM notarized height
     * @returns the last notarized height that has a MoM
     */
    int32_t komodo_prevMoMheight()
    {
        char symbol[KOMODO_ASSETCHAIN_MAXLEN];
        char dest[KOMODO_ASSETCHAIN_MAXLEN];
        komodo_state *sp;
        sp = &new_space::ks_new;
        //if ( (sp= komodo_stateptr(symbol,dest)) != 0 )
        {
            return sp->PrevMoMHeight();
        }
        return 0;
    }

    /****
     * Get the notarization data below a particular height
     * @param[in] nHeight the height desired
     * @param[out] notarized_hashp the hash of the notarized block
     * @param[out] notarized_desttxidp the desttxid
     * @returns the notarized height
     */
    int32_t komodo_notarizeddata(int32_t nHeight,uint256 *notarized_hashp,uint256 *notarized_desttxidp)
    {
        int32_t i=0;
        bool found = false;
        char symbol[KOMODO_ASSETCHAIN_MAXLEN];
        char dest[KOMODO_ASSETCHAIN_MAXLEN];
        komodo_state *sp;
        sp = &new_space::ks_new;
        //if ( (sp= komodo_stateptr(symbol,dest)) != nullptr )
        {
            return sp->NotarizedData(nHeight, notarized_hashp, notarized_desttxidp);
        }
        memset(notarized_hashp,0,sizeof(*notarized_hashp));
        memset(notarized_desttxidp,0,sizeof(*notarized_desttxidp));
        return 0;
    }
};

int main() {

    using std::chrono::high_resolution_clock;
    using std::chrono::duration_cast;
    using std::chrono::duration;
    using std::chrono::milliseconds;

    /* uint256 a("0x0ef6e4eab67aab0c780e94afdbd35c4285c26897aa374434570e2925a5e98f57");
    std::cout << a << std::endl;
    uint256 b;
    b = a;
    std::cout << b << std::endl;

    std::vector<uint256> va = {"0x0", "0x1"};
    for (auto a : va) std::cout << a << std::endl;

    struct notarized_checkpoint nc = {"0x000000000000000000000000000000000000000000000000000000000000dead", "0x0000000000000000000000000000000000000000000000000000000000000000", "0x0000000000000000000000000000000000000000000000000000000000000000", "0x0000000000000000000000000000000000000000000000000000000000000000", 0, 0, 0, 0, 0, 0, 0};
    std::cout << nc.notarized_hash << std::endl;

    std::cout << nc << std::endl; */

    size_t npoints_max =  sizeof(notarized_checkpoints)/sizeof(notarized_checkpoints[0]);
    std::cout << "Size: " << npoints_max;

    // [ !!! ] set the MoMdepth for tests [ !!! ]
    notarized_checkpoints[npoints_max-1].MoMdepth = 1;
    notarized_checkpoints[npoints_max-3].MoMdepth = 20;

    notarized_checkpoints[777].MoM = uint256("0xdead");
    notarized_checkpoints[777].MoMdepth = 1;

    // fill the old structures
    for (size_t idx = 0; idx < npoints_max; idx++)
    {
        old_space::komodo_notarized_update(old_space::sp,
        notarized_checkpoints[idx].nHeight,
        notarized_checkpoints[idx].notarized_height,
        notarized_checkpoints[idx].notarized_hash,
        notarized_checkpoints[idx].notarized_desttxid,
        notarized_checkpoints[idx].MoM,
        notarized_checkpoints[idx].MoMdepth);

        new_space::komodo_notarized_update(new_space::sp,
        notarized_checkpoints[idx].nHeight,
        notarized_checkpoints[idx].notarized_height,
        notarized_checkpoints[idx].notarized_hash,
        notarized_checkpoints[idx].notarized_desttxid,
        notarized_checkpoints[idx].MoM,
        notarized_checkpoints[idx].MoMdepth);
    }

    std::cout << " (" << old_space::sp->NUM_NPOINTS << ", " << new_space::sp->NumCheckpoints() << ")" << std::endl;

    int idx_old, idx_new;
    // 2524171 - FAIL
    int32_t arr_heights[] = { 2524224, 2524223, 2524210, 2524209, 2524200, 2524190, 2524171, 2524170 };
    size_t max_arr_heights = sizeof(arr_heights)/sizeof(arr_heights[0]);

    struct notarized_checkpoint *np_old, *np_new;

    for (size_t i = 0; i < max_arr_heights; i++) {

        np_old = old_space::komodo_npptr_for_height(arr_heights[i], &idx_old);
        std::cout << "[old] komodo_npptr_for_height(" << arr_heights[i] << "): ";

        // if (np_old)
        //     std::cout << np_old - old_space::sp->NPOINTS;
        // else
        //     std::cout << 0;
        // std::cout << " (idx_old = " << idx_old << ")" << std::endl;

        if (np_old)
            std::cout << *np_old << std::endl;
        else
            std::cout << "(null)" << std::endl;

        np_new = (notarized_checkpoint *)new_space::komodo_npptr(arr_heights[i]);

        std::cout << "[new] komodo_npptr(" << arr_heights[i] << "): ";

        if (np_new)
            std::cout << *np_new/* << std::endl*/;
        else
            std::cout << "(null)"/* << std::endl*/;

        if (np_old && np_new)
        {
            if (*np_old == *np_new)
                std::cout << " - " << CL_GRN "[ PASS ]" CL_N << std::endl;
            else
                std::cout << " - " << CL_RED "[ FAIL ]" CL_N << std::endl;
        }
        else
        {
            if (np_old == nullptr && np_new == nullptr)
                std::cout << " - " << CL_GRN "[ PASS ]" CL_N << std::endl;
            else
                std::cout << " - " << CL_RED "[ FAIL ]" CL_N << std::endl;
        }
        std::cout << std::endl;
    }

    /*
        DONE:

        old_space::komodo_npptr_for_height - new_space::komodo_npptr
        old_space::komodo_notarizeddata    - new_space::komodo_notarizeddata

        TODO:

        - komodo_notarized_height

    */

    // 2524223, 2524192, 2441342 - FAIL
    int32_t arr_nheights[] = { 2524224, 2524223, 2524210, 2524209, 2524200, 2524192, 2524190, 2524171, 2524170, 2441342 };
    //
    size_t max_arr_nheights = sizeof(arr_nheights)/sizeof(arr_nheights[0]);
    int32_t old_ret_height, new_ret_height;

    for (size_t i = 0; i < max_arr_nheights; i++) {
        uint256 ret_notarized_hashp, ret_notarized_desttxidp;
        std::cout << "[old] komodo_notarizeddata(" << arr_nheights[i] << ") = ";
        old_ret_height = old_space::komodo_notarizeddata(arr_nheights[i], &ret_notarized_hashp, &ret_notarized_desttxidp);
        std::cout << old_ret_height;
        std::cout << std::endl;

        std::cout << "[new] komodo_notarizeddata(" << arr_nheights[i] << ") = ";
        new_ret_height = new_space::komodo_notarizeddata(arr_nheights[i], &ret_notarized_hashp, &ret_notarized_desttxidp);
        std::cout << new_ret_height;
        std::cout << " - " << (old_ret_height == new_ret_height ? CL_GRN "[ PASS ]" CL_N : CL_RED "[ FAIL ]" CL_N);
        std::cout << std::endl;
    }


    np_old = old_space::komodo_npptr_at(0);

    std::cout << "komodo_npptr_at(" << (0) << ") = " << (*np_old) << std::endl;
    np_old = old_space::komodo_npptr_at(npoints_max-1);
    std::cout << "komodo_npptr_at(" << (npoints_max-1) << ") = " << (*np_old) << std::endl;
    np_old = old_space::komodo_npptr_at(777);
    std::cout << "komodo_npptr_at(" << (777) << ") = " << (*np_old) << std::endl;

    std::cout << "[old] komodo_prevMoMheight() = " << old_space::komodo_prevMoMheight() << std::endl;
    std::cout << "[new] komodo_prevMoMheight() = " << new_space::komodo_prevMoMheight() << std::endl;

    // full test (long)
    std::cout << "[ Test ] Start" << std::endl;
    auto t1 = high_resolution_clock::now();
    for (size_t i = 0; i < npoints_max; ++i)
    {
        int32_t nHeightLocal = notarized_checkpoints[i].nHeight;

        /* komodo_npptr */
        struct notarized_checkpoint *np_old, *np_new;
        int idx_old, idx_new;
        np_old = old_space::komodo_npptr_for_height(nHeightLocal, &idx_old);
        np_new = (notarized_checkpoint *)new_space::komodo_npptr(nHeightLocal);

        if (np_old && np_new)
            assert(*np_old == *np_new);
        else
            assert(np_old == nullptr && np_new == nullptr);

        /* komodo_notarizeddata */
        uint256 ret_notarized_hashp, ret_notarized_desttxidp;
        old_ret_height = old_space::komodo_notarizeddata(nHeightLocal, &ret_notarized_hashp, &ret_notarized_desttxidp);
        new_ret_height = new_space::komodo_notarizeddata(nHeightLocal, &ret_notarized_hashp, &ret_notarized_desttxidp);
        assert(old_ret_height == new_ret_height);
    }
    auto t2 = high_resolution_clock::now();
    std::cout << "[ Test ] End" << std::endl;

    // auto ms_int = duration_cast<milliseconds>(t2 - t1);
    duration<double, std::milli> ms_double = t2 - t1;
    std::cout << "Elapsed: " << CL_GRN << ms_double.count() << " ms" << CL_N << std::endl;
    return 0;
}