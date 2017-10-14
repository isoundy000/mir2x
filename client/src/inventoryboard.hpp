/*
 * =====================================================================================
 *
 *       Filename: inventoryboard.hpp
 *        Created: 10/08/2017 19:06:52
 *  Last Modified: 10/12/2017 13:33:31
 *
 *    Description: 
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

#pragma once
#include "widget.hpp"
#include "labelboard.hpp"
#include "tritexbutton.hpp"

class ProcessRun;
class InventoryBoard: public Widget
{
    private:
        LabelBoard m_GoldBoard;

    private:
        TritexButton m_CloseButton;

    private:
        ProcessRun *m_ProcessRun;

    public:
        InventoryBoard(int, int, ProcessRun *, Widget * = nullptr, bool = false);

    public:
        void DrawEx(int, int, int, int, int, int);

    public:
        bool ProcessEvent(const SDL_Event &, bool *);
};