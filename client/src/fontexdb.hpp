/*
 * =====================================================================================
 *
 *       Filename: fontexdb.h
 *        Created: 02/24/2016 17:51:16
 *  Last Modified: 02/26/2016 02:39:14
 *
 *    Description: this class only releases resource automatically
 *                 on loading new resources
 *
 *        Version: 1.0
 *       Revision: none
 *       Compiler: gcc
 *
 *         Author: ANHONG
 *          Email: anhonghe@gmail.com
 *   Organization: USTC
 *
 * =====================================================================================
 */

template<int N = 4>
class FontexDB final
{
    public:
        FontxDB()
            : m_SourceMaxCount(N * 256 * 2)
            , m_SourceCount(0)
        {
        }

        ~FontxDB()
        {
        }

    public:

        void Resize(int nMaxSize)
        {
            m_SourceMaxCount = std::max(N * 256 * 2, nMaxSize);
            while(m_SourceCount > m_SourceMaxCount){
                if(m_TimeStampQ.empty()){
                    // this won't happen, put it here for safity
                    break;
                }

                uint32_t nFontFaceKey = std::get<1>(m_TimeStampQ.front());
                uint32_t nUTF8Code    = std::get<2>(m_TimeStampQ.front());

                uint8_t nLCKey     = LinearCacheKey(nFontFaceKey, nUTF8Code);
                int     nTimeStamp = std::get<0>(m_TimeStampQ.front());

                ReleaseResource(nLCKey, nFontFaceKey, nUTF8Code, nTimeStamp);
            }
        }

    public:

        SDL_Texture *Retrieve(uint8_t nFileIndex, uint8_t nSize, uint8_t nStyle, uint32_t nUTF8Code)
        {
            // everytime when call this function
            // there must be a pointer retrieved responding to (nFontFaceKey, nUTF8Code)
            // even if it's nullptr, so update time stamp at each time
            //
            // overflow won't be a problem
            m_CurrentTime++;

            uint32_t nFontFaceKey = ((uint32_t)nFileIndex << 16) + ((uint32_t)nSize << 8) + nStyle;
            uint8_t  nLCacheKey   = LinearCacheKey(nFontFaceKey, nUTF8Code);

            int nLocationInLC;

            if(LocateInLinearCache(nLCacheKey, nFontFaceKey, nUTF8Code, nLocationInLC)){
                // find resource in LC, return it directly
                //
                // 1. move the record in LC to its head
                m_LCache[nLCacheKey].SwapHead(nLocationInLC);

                // 2. update access time stamp in LC *only*
                //    *important*:
                //    didn't update the access time in Cache!
                std::get<1>(m_LCache[nLCacheKey].Head()) = m_CurrentTime;

                // 3. push the access stamp at the end of the time stamp queue
                m_TimeStampQ.push({m_CurrentTime, nFontFaceKey, nUTF8Code});

                // 4. return the resource pointer
                return std::get<0>(m_LCache[nLCacheKey].Head());
            }else{
                // didn't find it in LC, try to find it in m_Cache
                auto pFontFaceInst = m_Cache.find(nFontFaceKey);
                if(pFontFaceInst != m_Cache.end()){
                    auto pTextureInst = pFontFaceInst.second.find(nUTF8Code);
                    if(pTextureInst != pFontFaceInst.second.end()){
                        // we find it in m_Cache!
                        //
                        // 1. Put a record in LC
                        //
                        //    but we need more operation when LC is already full
                        //    when it's full, insert a record cause drop an old record
                        //    we need the old record to update the last access time in m_Cache
                        if(m_LCache[nLCacheKey].Full()){
                            // since it's in LC, it must exist in m_Cache
                            int      nOldTimeStamp   = std::get<1>(m_LCache[nLCacheKey].Back());
                            uint32_t nOldFontFaceKey = std::get<2>(m_LCache[nLCacheKey].Back());
                            uint32_t nOldUTF8Key     = std::get<3>(m_LCache[nLCacheKey].Back());

                            m_Cache[nOldFontFaceKey][nOldUTF8Key].first = nOldTimeStamp;
                        }

                        m_LCache[nLCacheKey].PushHead(
                                {pTextureInst.second, m_CurrentTime, nFontFaceKey, nUTF8Code});

                        // 2. don't need to update access time in LC
                        //    set access time in m_Cache
                        //
                        pTextureInst.second.first = m_CurrentTime;

                        // 3. push the access stamp at the end of the time stamp queue
                        m_TimeStampQ.push({m_CurrentTime, nFontFaceKey, nUTF8Code});

                        // 4. return the resource pointer
                        return pTexture.second.second;
                    }
                }else{
                    // it's not in m_Cache, we need to load it from external source
                    auto pTexture = LoadTexture(nFileIndex, nSize, nStyle, nUTF8Code);

                    // 1. Put a record in LC
                    m_LCache[nLCacheKey].PushHead({pTexture, m_CurrentTime, nFontFaceKey, nUTF8Code});

                    // 2. put the resource in m_Cache
                    if(pFontFaceInst != m_Cache.end()){
                        pFontFaceInst.second[nUTF8Code] = {m_CurrentTime, pTexture};
                    }else{
                        m_Cache[nFontFaceKey][nUTF8Code] = {m_CurrentTime, pTexture};
                    }

                    // 3. push the access stamp at the end of the time stamp queue
                    m_TimeStampQ.push({m_CurrentTime, nFontFaceKey, nUTF8Code});

                    // 4. return the resource pointer
                    return pTexture;
                }
            }
        }

    private:

        // TODO
        // design it for better performance
        uint8_t LinearCacheKey(uint32_t nFontFaceKey, uint32_t nUTF8Code)
        {
            return (uint8_t)(nFontFaceKey + nUTF8Code);
        }

        TTF_Font *RetrieveFont(uint8_t nFileIndex, uint8_t nSize)
        {
            uint16_t nFontCode = ((uint16_t)nFileIndex << 8) + nSize;
            auto pFontInst = m_FontCache.find(nFontCode);
            if(pFontInst != m_FontCache.end()){
                // pFontInst.second may be nullptr
                // which means we tried to load it previously but faile
                // this helps to prevent trying to load it everytime
                return pFontInst.second;
            }

            auto pFont = TTF_OpenFont(m_FontFileNameV[nFileIndex].c_str(), nSize);
            m_FontCache[nFontCode] = pFont;
            return pFont;
        }

        SDL_Texture *LoadTexture(uint8_t nFileIndex, uint8_t nSize, uint8_t nStyle, uint32_t nUTF8Code)
        {
            auto *pFont = RetrieveFont(nFileIndex, nSize);
            if(pFont == nullptr){ return nullptr; }

            TTF_SetFontKerning(pFont, false);

            int nFontStyle = 0;
            if(nStyle & FONTSTYLE_BOLD){
                nFontStyle &= TTF_STYLE_BOLD;
            }

            if(nStyle & FONTSTYLE_ITALIC){
                nFontStyle &= TTF_STYLE_ITALIC;
            }

            if(nStyle & FONTSTYLE_UNDERLINE){
                nFontStyle &= TTF_STYLE_UNDERLINE;
            }

            if(nStyle & FONTSTYLE_STRIKETHROUGH){
                nFontStyle &= TTF_STYLE_STRIKETHROUGH;
            }

            TTF_SetFontStyle(pFont, nFontStyle);

            SDL_Surface *pSurface = nullptr;
            char szUTF8[8];

            *((uint32_t *)szUTF8) = nUTF8Code;
            szUTF8[4] = 0;

            if(nStyle & FONTSTYLE_SOLID){
                pSurface = TTF_RenderUTF8_Solid(pFont, szUTF8, {0XFF, 0XFF, 0XFF, 0XFF});
            }else if(stCharIdtor.FontInfo.Style & FONTSTYLE_SHADED){
                pSurface = TTF_RenderUTF8_Shaded(pFont, szUTF8, {0XFF, 0XFF, 0XFF, 0XFF}, {0X00, 0X00, 0X00, 0X00});
            }else{
                // blended is by default by of lowest priority
                // means if we really need to set SOLID/SHADOWED/BLENDED
                // most likely we don't want the default setting
                pSurface = TTF_RenderUTF8_Blended(pFont, szUTF8, {0XFF, 0XFF, 0XFF, 0XFF});
            }

            if(pSurface){
                SDL_Texture *pTexture = 
                    SDL_CreateTextureFromSurface(GetDeviceManager()->GetRenderer(), pSurface);
                SDL_FreeSurface(pSurface);
                return pTexture;
            }
            return nullptr;
        }

        // when calling this function, (nFontFaceKey, nUTF8Code):
        //
        // 1. shouldn't be in linear cache
        //
        // 2. it must be in m_Cache, only exception is m_CurrentTime overflowed
        //      a) when there is no overflow, everytime when free a resource, we compare the time
        //         if the time in m_Cache != time with the record in the queue, means there is 
        //         accessing later, so we keep it. other wise we free it.
        //
        //      b) when there is overflow, think about this scenario:
        //         current node in time stamp queue is at time k, but the last accessing time of the
        //         resource is at (k + max) = k, then it's equal, and we release it incorrectly
        //         
        void ReleaseResourceByTimeStamp(uint32_t nFontFaceKey, uint32_t nUTF8Code, int nTimeStamp)
        {
            auto pFontFaceInst = m_Cache.find(nFontFaceKey);
            if(pFontFaceInst != m_Cache.end()){
                auto pTextureInst = pFontFaceInst.second.find(nUTF8Code);
                if(pTextureInst != pFontFaceInst.second.end()){
                    if(pTextureInst.second.first == nTimeStamp){
                        SDL_DestroyTexture(pTextureInst.second.second);
                        pFontFaceInst.erase(pTextureInst);

                        m_ResourceCount--;
                    }
                }
            }
        }

        void ReleaseResource(uint8_t nLCKey, uint32_t nFontFaceKey, uint32_t nUTF8Code, int nTimeStamp)
        {
            // first check whether the resource is in LC
            // return directly if yes since we always keep resrouce in LC
            int nLocationInLC;
            if(LocateInLinearCache(nLCKey, nFontFaceKey, nUTF8Code, nLocationInLC)){ return; }

            // not in LC, release resource if (most likely) in the cache
            ReleaseResourceByTimeStamp(nFontFaceKey, nUTF8Code, nTimeStamp);
        }


        bool LocateInLinearCache(uint8_t nLCKey,
                uint32_t nFontFaceKey, uint32_t nUTF8Code, int &nLocationInLC)
        {
            for(m_LCache[nLCKey].Reset(); !m_LCache[nLCKey].Done(); m_LCache[nLCKey].Forward()){
                if(std::get<1>(m_LCache[nLCKey].Current()) == nFontFaceKey
                        && std::get<2>(m_LCache[nLCKey].Current()) == nUTF8Code){
                    nLocationInLC =  m_LCache[nLCKey].CurrentIndex();
                    return true;
                }
            }
            return false;
        }


    private:
        std::tuple<uint8_t, uint8_t, TTF_Font *> m_FontCache[256][5];
};