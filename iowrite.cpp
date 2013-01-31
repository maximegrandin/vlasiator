#include <cstdlib>
#include <iostream>
#include <iomanip> // for setprecision()
#include <cmath>
#include <vector>
#include <sstream>
#include <ctime>
#include "iowrite.h"
#include "phiprof.hpp"
#include "parameters.h"
#include "logger.h"
#include "vlsvwriter2.h"
#include "vlsvreader2.h"
#include "vlasovmover.h"

using namespace std;
using namespace phiprof;

extern Logger logFile, diagnostic;

typedef Parameters P;


bool writeDataReducer(const dccrg::Dccrg<SpatialCell>& mpiGrid,
                      DataReducer& dataReducer,
                      int dataReducerIndex,
                      VLSVWriter& vlsvWriter){
   map<string,string> attribs;                      
   string variableName,dataType;
   bool success=true;
   
   uint dataSize,vectorSize;
   attribs["mesh"] = "SpatialGrid";
   variableName = dataReducer.getName(dataReducerIndex);
   if (dataReducer.getDataVectorInfo(dataReducerIndex,dataType,dataSize,vectorSize) == false) {
      cerr << "ERROR when requesting info from DRO " << dataReducerIndex << endl;
      return false;
   }
   vector<uint64_t> cells = mpiGrid.get_cells();
   uint64_t arraySize = cells.size()*vectorSize*dataSize;
   
   //Request DataReductionOperator to calculate the reduced data for all local cells:
   char* varBuffer = new char[arraySize];
   for (uint64_t cell=0; cell<cells.size(); ++cell) {
      if (dataReducer.reduceData(mpiGrid[cells[cell]],dataReducerIndex,varBuffer + cell*vectorSize*dataSize) == false){
         success = false;
      }
      
      if (success == false){
         logFile << "(MAIN) writeGrid: ERROR datareductionoperator '" << dataReducer.getName(dataReducerIndex) <<
            "' returned false!" << endl << writeVerbose;
      }
   }
   // Write  reduced data to file if DROP was successful:
   if(success){
      if (vlsvWriter.writeArray("VARIABLE",variableName,attribs,cells.size(),vectorSize,dataType,dataSize,varBuffer) == false)
         success = false;
      if (success == false){
         logFile << "(MAIN) writeGrid: ERROR failed to write datareductionoperator data to file!" << endl << writeVerbose;
      }
   }
   delete[] varBuffer;
   varBuffer = NULL;
   return success;
}

template <typename T>
bool writeScalarParameter(string name,T value,VLSVWriter& vlsvWriter,int masterRank,MPI_Comm comm){
   int myRank;
   MPI_Comm_rank(comm,&myRank);
   if(myRank==masterRank){
      map<string,string> attribs;
      std::ostringstream s;
      s << value;
      attribs["value"]=s.str();
      vlsvWriter.writeArrayMaster("PARAMETERS",name,attribs,1,1,&value);
   }
   return true;
}

bool writeGrid(const dccrg::Dccrg<SpatialCell>& mpiGrid,
               DataReducer& dataReducer,
               const string& name,
               const uint& index,
               const bool& writeRestart) {
    double allStart = MPI_Wtime();
    bool success = true;
    int myRank;

    MPI_Comm_rank(MPI_COMM_WORLD,&myRank);
    if(writeRestart)
        phiprof::start("writeGrid-restart");
    else
        phiprof::start("writeGrid-reduced");
    
   // Create a name for the output file and open it with VLSVWriter:
   stringstream fname;
   fname << name <<".";
   fname.width(7);
   fname.fill('0');
   fname << index << ".vlsv";
   
   VLSVWriter vlsvWriter;
   vlsvWriter.open(fname.str(),MPI_COMM_WORLD,0);
   
   // Get all local cell Ids and write to file:
   map<string,string> attribs;
   vector<uint64_t> cells = mpiGrid.get_cells();

   attribs.clear();
   if (vlsvWriter.writeArray("MESH","SpatialGrid",attribs,cells.size(),1,&(cells[0])) == false) {
      cerr << "Proc #" << myRank << " failed to write cell Ids!" << endl;
   }

   // Create a buffer for spatial cell coordinates. Copy all coordinates to 
   // buffer and write:
   Real* buffer = new Real[6*cells.size()];
   for (size_t i=0; i<cells.size(); ++i) {
      SpatialCell* SC = mpiGrid[cells[i]];
      for (int j=0; j<6; ++j) {
	 buffer[6*i+j] = SC->parameters[j];
      }
   }
   attribs.clear();
   if (vlsvWriter.writeArray("COORDS","SpatialGrid",attribs,cells.size(),6,buffer) == false) {
      cerr << "Proc #" << myRank << " failed to write cell coords!" << endl;
   }
   delete[] buffer;   
   
   
   writeScalarParameter("t",P::t,vlsvWriter,0,MPI_COMM_WORLD);
   writeScalarParameter("dt",P::dt,vlsvWriter,0,MPI_COMM_WORLD);
   writeScalarParameter("tstep",P::tstep,vlsvWriter,0,MPI_COMM_WORLD);
   writeScalarParameter("fileIndex",index,vlsvWriter,0,MPI_COMM_WORLD);
   writeScalarParameter("xmin",P::xmin,vlsvWriter,0,MPI_COMM_WORLD);
   writeScalarParameter("xmax",P::xmax,vlsvWriter,0,MPI_COMM_WORLD);
   writeScalarParameter("ymin",P::ymin,vlsvWriter,0,MPI_COMM_WORLD);
   writeScalarParameter("ymax",P::ymax,vlsvWriter,0,MPI_COMM_WORLD);
   writeScalarParameter("zmin",P::zmin,vlsvWriter,0,MPI_COMM_WORLD);
   writeScalarParameter("zmax",P::zmax,vlsvWriter,0,MPI_COMM_WORLD);
   writeScalarParameter("xcells_ini",P::xcells_ini,vlsvWriter,0,MPI_COMM_WORLD);
   writeScalarParameter("ycells_ini",P::ycells_ini,vlsvWriter,0,MPI_COMM_WORLD);
   writeScalarParameter("zcells_ini",P::zcells_ini,vlsvWriter,0,MPI_COMM_WORLD);
   writeScalarParameter("vxmin",P::vxmin,vlsvWriter,0,MPI_COMM_WORLD);
   writeScalarParameter("vxmax",P::vxmax,vlsvWriter,0,MPI_COMM_WORLD);
   writeScalarParameter("vymin",P::vymin,vlsvWriter,0,MPI_COMM_WORLD);
   writeScalarParameter("vymax",P::vymax,vlsvWriter,0,MPI_COMM_WORLD);
   writeScalarParameter("vzmin",P::vzmin,vlsvWriter,0,MPI_COMM_WORLD);
   writeScalarParameter("vzmax",P::vzmax,vlsvWriter,0,MPI_COMM_WORLD);
   writeScalarParameter("vxblocks_ini",P::vxblocks_ini,vlsvWriter,0,MPI_COMM_WORLD);
   writeScalarParameter("vyblocks_ini",P::vyblocks_ini,vlsvWriter,0,MPI_COMM_WORLD);
   writeScalarParameter("vzblocks_ini",P::vzblocks_ini,vlsvWriter,0,MPI_COMM_WORLD);
   
   if(writeRestart == false ) {
      // Write variables calculate d by DataReductionOperators (DRO). We do not know how many 
      // numbers each DRO calculates, so a buffer has to be re-allocated for each DRO:
      for (uint i=0; i<dataReducer.size(); ++i) {
         writeDataReducer(mpiGrid,dataReducer,i,vlsvWriter);
      }
      
      phiprof::initializeTimer("Barrier","MPI","Barrier");
      phiprof::start("Barrier");
      MPI_Barrier(MPI_COMM_WORLD);
      phiprof::stop("Barrier");
      vlsvWriter.close();
      phiprof::stop("writeGrid-reduced");
   }
   else {
      //write restart
      uint64_t totalBlocks = 0;  
      for(size_t cell=0;cell<cells.size();++cell){
         totalBlocks+=mpiGrid[cells[cell]]->size();
      }
      //write out DROs we need for restarts
      DataReducer restartReducer;
      restartReducer.addOperator(new DRO::DataReductionOperatorCellParams("background_B",CellParams::BGBX,3));
      restartReducer.addOperator(new DRO::DataReductionOperatorCellParams("perturbed_B",CellParams::PERBX,3));
      restartReducer.addOperator(new DRO::DataReductionOperatorCellParams("moments",CellParams::RHO,4));
      restartReducer.addOperator(new DRO::DataReductionOperatorCellParams("moments_dt2",CellParams::RHO_DT2,4));
      restartReducer.addOperator(new DRO::DataReductionOperatorCellParams("moments_r",CellParams::RHO_R,4));
      restartReducer.addOperator(new DRO::DataReductionOperatorCellParams("moments_v",CellParams::RHO_V,4));
      restartReducer.addOperator(new DRO::Blocks);
      restartReducer.addOperator(new DRO::MPIrank);
      restartReducer.addOperator(new DRO::BoundaryType);
      restartReducer.addOperator(new DRO::BoundaryLayer);
      restartReducer.addOperator(new DRO::VelocitySubSteps);

      for (uint i=0; i<restartReducer.size(); ++i) {
         writeDataReducer(mpiGrid,restartReducer,i,vlsvWriter);
      }
      
      // Write velocity blocks and related data. 
      // In restart we just write velocity grids for all cells.
      // First write global Ids of those cells which write velocity blocks (here: all cells):
      if (vlsvWriter.writeArray("CELLSWITHBLOCKS","SpatialGrid",attribs,cells.size(),1,&(cells[0])) == false) success = false;
      if (success == false) logFile << "(MAIN) writeGrid: ERROR failed to write CELLSWITHBLOCKS to file!" << endl << writeVerbose;
      //Write velocity block coordinates.
      std::vector<Real> velocityBlockParameters;
      velocityBlockParameters.reserve(totalBlocks*BlockParams::N_VELOCITY_BLOCK_PARAMS);

      
      //gather data for writing
      for (size_t cell=0; cell<cells.size(); ++cell) {
         int index=0;
         SpatialCell* SC = mpiGrid[cells[cell]];
         for (unsigned int block_i=0;block_i < SC->number_of_blocks;block_i++){
            unsigned int block = SC->velocity_block_list[block_i];
            Velocity_Block* block_data = SC->at(block);
            for(unsigned int p=0;p<BlockParams::N_VELOCITY_BLOCK_PARAMS;++p){
               velocityBlockParameters.push_back(block_data->parameters[p]);
            }
         }
      }
      
      attribs.clear();
      if (vlsvWriter.writeArray("BLOCKCOORDINATES","SpatialGrid",attribs,totalBlocks,BlockParams::N_VELOCITY_BLOCK_PARAMS,&(velocityBlockParameters[0])) == false) success = false;
      if (success == false) logFile << "(MAIN) writeGrid: ERROR failed to write BLOCKCOORDINATES to file!" << endl << writeVerbose;
      velocityBlockParameters.clear();

   
      // Write values of distribution function:
      std::vector<Real> velocityBlockData;
      velocityBlockData.reserve(totalBlocks*SIZE_VELBLOCK);
   
      for (size_t cell=0; cell<cells.size(); ++cell) {
         int index=0;
         SpatialCell* SC = mpiGrid[cells[cell]];
         for (unsigned int block_i=0;block_i < SC->number_of_blocks;block_i++){
            unsigned int block = SC->velocity_block_list[block_i];
            Velocity_Block* block_data = SC->at(block);
            for(unsigned int vc=0;vc<SIZE_VELBLOCK;++vc){
               velocityBlockData.push_back(block_data->data[vc]);
            }
         }
      }
      
      attribs.clear();
      attribs["mesh"] = "SpatialGrid";
      if (vlsvWriter.writeArray("BLOCKVARIABLE","avgs",attribs,totalBlocks,SIZE_VELBLOCK,&(velocityBlockData[0])) == false) success=false;
      if (success ==false)      logFile << "(MAIN) writeGrid: ERROR occurred when writing BLOCKVARIABLE avgs" << endl << writeVerbose;
      velocityBlockData.clear();
      vlsvWriter.close();

      phiprof::stop("writeGrid-restart");//,1.0e-6*bytesWritten,"MB");

   }

   return success;
}
   


bool writeDiagnostic(const dccrg::Dccrg<SpatialCell>& mpiGrid,
                     DataReducer& dataReducer)
{
   int myRank;
   MPI_Comm_rank(MPI_COMM_WORLD,&myRank);
   
   string dataType;
   uint dataSize, vectorSize;
   vector<uint64_t> cells = mpiGrid.get_cells();
   cuint nCells = cells.size();
   cuint nOps = dataReducer.size();
   vector<Real> localMin(nOps), localMax(nOps), localSum(nOps+1), localAvg(nOps),
               globalMin(nOps),globalMax(nOps),globalSum(nOps+1),globalAvg(nOps);
   localSum[0] = 1.0 * nCells;
   Real buffer;
   bool success = true;
   static bool printDiagnosticHeader = true;
   
   if (printDiagnosticHeader == true && myRank == MASTER_RANK) {
      if (P::isRestart){
         diagnostic << "# ==== Restart from file "<< P::restartFileName << " ===="<<endl;
      }
      diagnostic << "# Column 1 Step" << endl;
      diagnostic << "# Column 2 Simulation time" << endl;
      diagnostic << "# Column 3 Time step dt" << endl;
      for (uint i=0; i<nOps; ++i) {
         diagnostic << "# Columns " << 4 + i*4 << " to " << 7 + i*4 << ": " << dataReducer.getName(i) << " min max sum average" << endl;
      }
      printDiagnosticHeader = false;
   }
   
   for (uint i=0; i<nOps; ++i) {
      
      if (dataReducer.getDataVectorInfo(i,dataType,dataSize,vectorSize) == false) {
         cerr << "ERROR when requesting info from diagnostic DRO " << dataReducer.getName(i) << endl;
      }
      localMin[i] = std::numeric_limits<Real>::max();
      localMax[i] = std::numeric_limits<Real>::min();
      localSum[i+1] = 0.0;
      buffer = 0.0;
      
      // Request DataReductionOperator to calculate the reduced data for all local cells:
      for (uint64_t cell=0; cell<nCells; ++cell) {
         success = true;
         if (dataReducer.reduceData(mpiGrid[cells[cell]], i, &buffer) == false) success = false;
         localMin[i] = min(buffer, localMin[i]);
         localMax[i] = max(buffer, localMax[i]);
         localSum[i+1] += buffer;
      }
      localAvg[i] = localSum[i+1];
      
      if (success == false) logFile << "(MAIN) writeDiagnostic: ERROR datareductionoperator '" << dataReducer.getName(i) <<
                               "' returned false!" << endl << writeVerbose;
   }
   
   MPI_Reduce(&localMin[0], &globalMin[0], nOps, MPI_Type<Real>(), MPI_MIN, 0, MPI_COMM_WORLD);
   MPI_Reduce(&localMax[0], &globalMax[0], nOps, MPI_Type<Real>(), MPI_MAX, 0, MPI_COMM_WORLD);
   MPI_Reduce(&localSum[0], &globalSum[0], nOps + 1, MPI_Type<Real>(), MPI_SUM, 0, MPI_COMM_WORLD);
   
   diagnostic << setprecision(12); 
   diagnostic << Parameters::tstep << "\t";
   diagnostic << Parameters::t << "\t";
   diagnostic << Parameters::dt << "\t";
   
   for (uint i=0; i<nOps; ++i) {
      if (globalSum[0] != 0.0) globalAvg[i] = globalSum[i+1] / globalSum[0];
      else globalAvg[i] = globalSum[i+1];
      if (myRank == MASTER_RANK) {
         diagnostic << globalMin[i] << "\t" <<
         globalMax[i] << "\t" <<
         globalSum[i+1] << "\t" <<
         globalAvg[i] << "\t";
      }
   }
   if (myRank == MASTER_RANK) diagnostic << endl << write;
   return true;
}


