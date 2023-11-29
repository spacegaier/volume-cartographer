// CVolumeViewer.h
// Chao Du 2015 April
#pragma once

#include "CImageViewer.hpp"

namespace ChaoVis
{

class CVolumeViewer : public CImageViewer
{
    Q_OBJECT

public:
    enum EViewState {
        ViewStateEdit,  // edit mode
        ViewStateDraw,  // draw mode
        ViewStateIdle   // idle mode
    };

    CVolumeViewer(QWidget* parent = 0);

    void SetViewState(EViewState nViewState) { fViewState = nViewState; }
    EViewState GetViewState(void) { return fViewState; }

protected:
    // data
    EViewState fViewState;
    int sliceIndexToolStart{-1};
};  // class CVolumeViewer

}  // namespace ChaoVis
