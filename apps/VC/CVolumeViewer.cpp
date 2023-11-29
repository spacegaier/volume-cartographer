// CVolumeViewer.cpp
// Chao Du 2015 April
#include "CVolumeViewer.hpp"
#include "HBase.hpp"

using namespace ChaoVis;
using qga = QGuiApplication;

// Constructor
CVolumeViewer::CVolumeViewer(QWidget* parent)
    : CImageViewer(parent)
    , fViewState(EViewState::ViewStateIdle)
{
    fNextBtn->setText(tr("Next Slice"));
    fPrevBtn->setText(tr("Previous Slice"));
}