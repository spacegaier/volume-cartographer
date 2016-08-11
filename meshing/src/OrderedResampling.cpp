//
// Created by Hannah Hatch on 7/25/16.
//

#include "meshing/OrderedResampling.h"
#include "meshing/CalculateNormals.h"

using namespace volcart::meshing;

//// Constructors ////
OrderedResampling::OrderedResampling()
{
    _inWidth = 0;
    _inHeight = 0;
}//constructor

OrderedResampling::OrderedResampling(VC_MeshType::Pointer mesh, int in_width, int in_height)
{
    _input = mesh;
    _inWidth  = in_width ;
    _inHeight = in_height;
}//constructor with parameters

//// Set Inputs/Get Output ////
void OrderedResampling::setMesh(VC_MeshType::Pointer mesh, int in_width, int in_height)
{
    _input = mesh;
    _inWidth = in_width;
    _inHeight = in_height;
}//setParameters

VC_MeshType::Pointer OrderedResampling::getOutputMesh()
{
    if(_output.IsNull())
    {
        std::cerr << "Error: Output Mesh is not set" << std::endl;
        return NULL;
    }
    else
        return _output;
}//getOutput

int OrderedResampling::getOutputWidth()  { return _outWidth; };
int OrderedResampling::getOutputHeight() { return _outHeight; };

///// Processing /////
void OrderedResampling::compute()
{
    _output = VC_MeshType::New();

    // Dimensions of resampled, ordered mesh
    _outWidth  = ( _inWidth  + 1 ) / 2;
    _outHeight = ( _inHeight + 1 ) / 2;

    //Tells the loop whether or not the points in that line should be added to the new mesh
    bool line_skip = false;

    //Loop iterator
    unsigned long k = 0;
    VC_PointsInMeshIterator pointsIterator = _input->GetPoints()->Begin();

    // Adds certain points from old mesh into the new mesh
    for( ; pointsIterator != _input->GetPoints()->End(); pointsIterator++, k++) {

        // If we've reached the end of a row, reset k and flip the line_skip bool
        if ( k == _inWidth ) {
            k = 0;
            line_skip = !line_skip;
        }

        // Skip this point if we're skipping this line
        if( line_skip ) continue;

        // Only add every other point
        if ( k % 2 == 0 ) _output->SetPoint( _output->GetNumberOfPoints(), pointsIterator.Value() );

    }

    // Something went wrong with resampling. Number of points aren't what we expect...
    assert( _output->GetNumberOfPoints() == _outWidth * _outHeight && "Error resampling. Output and expected output don't match.");

    // Vertices for each face in the new mesh
    unsigned long point1, point2, point3, point4;

    // Create two new faces each iteration based on new set of points and keeps normals same as original
    for(unsigned long i = 0; i < _outHeight - 1; i++) {
        for (unsigned long j = 0; j < _outWidth - 1; j++) {

            //4 points allows us to create the upper and lower faces at the same time
            point1 = i * _outWidth + j;
            point2 = point1 + 1;
            point3 = point2 + _outWidth;
            point4 = point3 - 1;

            if( point1 >= _output->GetNumberOfPoints() ||
                point2 >= _output->GetNumberOfPoints() ||
                point3 >= _output->GetNumberOfPoints() ||
                point4 >= _output->GetNumberOfPoints() ) {
                throw std::out_of_range("Predicted vertex index for face generation out of range of point set.");
            }

            //Add both these faces to the mesh
            _addCell( point2, point3, point4 );
            _addCell( point1, point2, point4 );

        }
    }

    volcart::meshing::CalculateNormals calcNorm(_output);
    calcNorm.compute();
    _output = calcNorm.getMesh();

    std::cerr << "volcart::meshing::OrderedResampling: Points in resampled mesh "<< _output->GetNumberOfPoints() << std::endl;
    std::cerr << "volcart::meshing::OrderedResampling: Cells in resampled mesh "<< _output->GetNumberOfCells() << std::endl;

}//compute

void OrderedResampling::_addCell( unsigned long a, unsigned long b, unsigned long c )
{
    VC_CellType::CellAutoPointer current_C;

    current_C.TakeOwnership(new VC_TriangleType);

    current_C->SetPointId(0, a);
    current_C->SetPointId(1, b);
    current_C->SetPointId(2, c);

    _output->SetCell( _output->GetNumberOfCells(), current_C );
}//addCell