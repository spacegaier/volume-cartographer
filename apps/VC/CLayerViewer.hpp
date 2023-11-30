// CLayerViewer.hpp
// Philip Allgaier 2023 November
#pragma once

#include "CImageViewer.hpp"

#include "vc/core/types/PerPixelMap.hpp"

namespace ChaoVis
{

class CLayerViewer : public CImageViewer
{
    Q_OBJECT

public:
    CLayerViewer(QWidget* parent = 0);

    void setPPM(volcart::PerPixelMap input) { ppm = input; }
    void showCurveForSlice(int sliceIndex);

    void setProgress(int progress);
    void setProgressText(const QString& text);

    QString getTempPath() const { return tempDir.path(); }

signals:
    void SendSignalCancelLayerGeneration();

protected:
    std::string segID;
    volcart::PerPixelMap ppm;

    QTemporaryDir tempDir{};
    QProgressBar* progressBar;
    QPushButton* btnCancel;
};

}  // namespace ChaoVis
