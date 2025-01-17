
//
//
//    Copyright (C) 2021 Universitat de València - UV
//    Copyright (C) 2021 Universitat Politècnica de València - UPV
//
//    This file is part of PenRed: Parallel Engine for Radiation Energy Deposition.
//
//    PenRed is free software: you can redistribute it and/or modify
//    it under the terms of the GNU Affero General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    PenRed is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU Affero General Public License for more details.
//
//    You should have received a copy of the GNU Affero General Public License
//    along with PenRed.  If not, see <https://www.gnu.org/licenses/>. 
//
//    contact emails:
//
//        vicent.gimenez.alventosa@gmail.com (Vicent Giménez Alventosa)
//        vicente.gimenez@uv.es (Vicente Giménez Gómez)
//    
//

#ifdef _PEN_USE_DICOM_

#include "image_spatialSampling.hh"

void image_spatialSampling::geoSampling(double pos[3], pen_rand& random) const{

  //Obtain voxel index randomly using Walker's aliasing algorithm
  long int ivox = IRND(F,K,nvox,random);

  long int ix = ivox % nx;
  long int iy = (ivox % nxy)/nx;
  long int iz = ivox / nxy;
  
  pos[0] = dx*(static_cast<double>(ix) + 0.5 + (1.0-2.0*random.rand())*0.5)-imageCx;
  pos[1] = dy*(static_cast<double>(iy) + 0.5 + (1.0-2.0*random.rand())*0.5)-imageCy;
  pos[2] = dz*(static_cast<double>(iz) + 0.5 + (1.0-2.0*random.rand())*0.5)-imageCz;

  if(savegeosampling){
    printf("tallyimage_spatialSampling:\n");
    printf("                          : voxel index   :   %ld\n",ivox);
    printf("                          : source chosen :  (%3ld,%3ld,%3ld)\n",ix,iy,iz);
    printf("                          : center (image source coordinate system):  (%9.3lf,%9.3lf,%9.3lf)\n",
    dx*static_cast<double>(ix)+Ox-dx/2-isocenter[0],
    dy*static_cast<double>(iy)+Oy-dy/2-isocenter[1],
    dz*static_cast<double>(iz)+Oz-dz/2-isocenter[2]);
    printf("                          : Position (phantom coordinate system): (%9.3lf,%9.3lf,%9.3lf)\n",
    pos[0]+translation[0],pos[1]+translation[1],pos[2]+translation[2]);fflush(stdout);
  }
  
}

int image_spatialSampling::configure(const pen_parserSection& config,
				     const unsigned verboseConf){

  verbose = verboseConf;

  int err = 0;

  isocenterph[0] = 0.0;
  isocenterph[1] = 0.0;
  isocenterph[2] = 0.0;

  err = config.read("savesampling",savegeosampling);
  if(err != INTDATA_SUCCESS){
    savegeosampling = false;
  }
  if(verbose > 1){
      if(savegeosampling){
        printf("image_spatialSampling:configure: the source position sampling will be saved.\n");
      }
      else
      {
        printf("image_spatialSampling:configure: the source position sampling will not be saved.\n");        
      }
  }

  // Read DICOM directory path
  //****************************
  std::string directoryPath;
  if(config.read("directory",directoryPath) != INTDATA_SUCCESS){
    if(verbose > 0){
      printf("image_spatialSampling:configure:Error: Unable to read field "
	     "'directory'. String spected.\n");
    }
    return 1;
  }

  err = config.read("isocenter/x",isocenter[0]);
  if(err != INTDATA_SUCCESS){
    isocenter[0] = 0.0;
  }

  err = config.read("isocenter/y",isocenter[1]);
  if(err != INTDATA_SUCCESS){
    isocenter[1] = 0.0;
  }

  err = config.read("isocenter/z",isocenter[2]);
  if(err != INTDATA_SUCCESS){
    isocenter[2] = 0.0;    
  }

  err = config.read("position/x",translation[0]);
  if(err != INTDATA_SUCCESS){
    translation[0] = 0.0;
    positionset = false;
  }

  err = config.read("position/y",translation[1]);
  if(err != INTDATA_SUCCESS){
    translation[1] = 0.0;
    positionset = false;
  }

  err = config.read("position/z",translation[2]);
  if(err != INTDATA_SUCCESS){
    translation[2] = 0.0;
    positionset = false;    
  }

  bool toRotate = false;
  double omega, theta, phi;
  err = config.read("euler/omega",omega);
  if(err != INTDATA_SUCCESS){
    omega = 0.0;
  }else toRotate = true;

  err = config.read("euler/theta",theta);
  if(err != INTDATA_SUCCESS){
    theta = 0.0;
  }else toRotate = true;

  err = config.read("euler/phi",phi);
  if(err != INTDATA_SUCCESS){
    phi = 0.0;
  }else toRotate = true;

  //Create the rotation
  if(toRotate)
    setRotationZYZ(omega,theta,phi);

  // Try to load the DICOM
  //************************
  pen_dicom dicom;
  err = dicom.loadDicom(directoryPath.c_str(),verbose);
  if(err != PEN_DICOM_SUCCESS){
    if(verbose > 0){
      printf("image_spatialSampling:configure: "
	     "Error loading DICOM '%s'\n",directoryPath.c_str());
      printf("                 Error code: %d\n",err);
    }
    return 2;
  }  

  //Store image dimensions
  nx = dicom.getNX();
  ny = dicom.getNY();
  nz = dicom.getNZ();
  nxy = nx*ny;
  nvox = nxy*nz;

  dx = dicom.getDX();
  dy = dicom.getDY();
  dz = dicom.getDZ();

  imageCx = static_cast<double>(nx)*dx*0.5;
  imageCy = static_cast<double>(ny)*dy*0.5;
  imageCz = static_cast<double>(nz)*dz*0.5;

  Ox = dicom.getOriginX();
  Oy = dicom.getOriginY();
  Oz = dicom.getOriginZ();

  if(!positionset){

    err = config.read("phantom-isocenter/x",isocenterph[0]);
    if(err != INTDATA_SUCCESS){
      isocenterph[0] = 0.0;
    }

    err = config.read("phantom-isocenter/y",isocenterph[1]);
    if(err != INTDATA_SUCCESS){
      isocenterph[1] = 0.0;
    }

    err = config.read("phantom-isocenter/z",isocenterph[2]);
    if(err != INTDATA_SUCCESS){
      isocenterph[2] = 0.0;    
    }

    //Set translation to isocenter until geometry assign
    translation[0] -= isocenter[0];
    translation[1] -= isocenter[1];
    translation[2] -= isocenter[2];
  }
  else
  {
    translation[0] -= isocenter[0];
    translation[1] -= isocenter[1];
    translation[2] -= isocenter[2];    
  }

  std::string walkersfilename;
  bool computeWalker = true;
  if(config.read("ActivityDistribution/read",walkersfilename) != INTDATA_SUCCESS){
    if(verbose > 0){
      printf("image_spatialSampling:configure: no ActivityDistribution data file especified.\n"
         "Walker algorithm aliases will be calculated.\n");
    }
  }
  else
  {
    computeWalker = false;
    if(verbose > 0){
      printf("image_spatialSampling:configure: ActivityDistribution data file especified.\n"
         "Walker algorithm aliases will be read from %s.\n",walkersfilename.c_str());
    }    
  }
 
 //Get raw image information
  const double* image = dicom.readImage();

  F = static_cast<double*>(malloc(sizeof(double)*nvox));
  K = static_cast<long int*>(malloc(sizeof(long int)*nvox));

  if(F == nullptr){
    if(verbose > 0)
      printf("image_spatialSampling:configure: "
	     "Unable to allocate memory for 'F' array\n");
    return 3;
  }

  if(K == nullptr){
    if(verbose > 0)
      printf("image_spatialSampling:configure: "
	     "Unable to allocate memory for 'K' array\n");
    return 4;
  }

  if(computeWalker)
  {
    //Init walker algorithm
    printf("Initializing Walker algorithm ... ");fflush(stdout);
    IRND0(image,F,K,nvox);
    printf("done\n");fflush(stdout); 
  }
  else
  {
    //Init walker algorithm
    printf("Reading initialization Walker algorithm ... ");fflush(stdout);
    char buffer[81];
    FILE* in;
    char BLINE[100];  
  
    strcpy(buffer,walkersfilename.c_str());
    in = fopen(buffer,"r");
    if(in == NULL){
      printf("\n*************************************************************************\n");
      printf("image_spatialSampling: Error: Cannot open Walker initialization data file\n");
      printf("*************************************************************************\n");
      fclose(in);                            
      return 5;                      
    }

    for(int i=1; i<=8; i++){
       BLINE[0]='\0';
       fscanf(in,"%[^\r\n]%*[^\n]",BLINE);
       if(fgetc(in)=='\r'){fgetc(in);}
    }
    for(long int i = 0; i < nvox; i++){
      //int Num_Elements_Llegits = sscanf(BLINE, "%8ld  %16.8E", &K[i], &F[i]);
      //fprintf(out," %8ld  %16.8E\n",K[i],F[i]);
      //fscanf(in,"%ld %lf%*[^\n]", &K[i], &F[i]);getc(in);
      fscanf(in,"%ld %lf", &K[i], &F[i]);
    }

    fclose(in);
    in = nullptr; 
    printf("done\n");fflush(stdout); 

  }

  std::string walkersoutfilename;
  if(config.read("ActivityDistribution/save",walkersoutfilename) != INTDATA_SUCCESS){
    if(verbose > 0){
      printf("image_spatialSampling:configure: the ActivityDistribution data will not be saved.\n"
         "Walker algorithm aliases will be calculated.\n");
    }
  }
  else
  {
    if(verbose > 0){
      printf("image_spatialSampling:configure: the ActivityDistribution data will be saved to %s.\n",walkersoutfilename.c_str());
    }    
    char buffer[81];
    FILE* out;
  
    strcpy(buffer,walkersoutfilename.c_str());
     
    out = fopen(buffer,"w");
    if (out == NULL)
    {
      printf("\n********************************************************************************\n");
      printf("image_spatialSampling: Error: Cannot open Walker initialization output data file\n");
      printf("********************************************************************************\n");
      fclose(out);                            
      return 6;                      
    }
    
    
    // Write header 
    fprintf(out,"#------------------------------------------------------------\n");
    fprintf(out,"# PenRed: Walker aliasing algorithm\n");
    fprintf(out,"# Cutoff and alias values\n");

    fprintf(out,"#\n");
    fprintf(out,"# No. of voxels in x,y,z directions and total:\n");
    fprintf(out,"#  %ld %ld %ld %ld\n",nx,ny,nz,nvox);
    fprintf(out,"#\n");
    fprintf(out,"# ");
    fprintf(out,"Alias    : ");
    fprintf(out,"Cutoff   \n");
        
    //Write data    
    for(long int i = 0; i < nvox; i++)
    {
      fprintf(out," %8ld  %16.8E\n",K[i],F[i]);
    }

    fclose(out);
    out = nullptr;
    printf("done\n");fflush(stdout); 
 }

    
  if(verbose > 1){
    if(!positionset){
      printf("Source image position not set\n");
      printf("\n Source attached to a phantom image with isocenter:\n");
      printf("Phantom Isocenter (Isox,Isoy,Isoz):\n %12.4E %12.4E %12.4E\n",isocenterph[0],isocenterph[1],isocenterph[2]);
    }
    else
    {
      printf("Source image position set\n");      
    }
    printf("\n Image Origin (Ox,Oy,Oz):\n %12.4E %12.4E %12.4E\n",
     Ox,Oy,Oz);fflush(stdout);
    printf("Image Isocenter (Isox,Isoy,Isoz):\n %12.4E %12.4E %12.4E\n",
     isocenter[0],isocenter[1],isocenter[2]);fflush(stdout);
    printf("Image center (x,y,z):\n %12.4E %12.4E %12.4E\n",
	   translation[0],translation[1],translation[2]);fflush(stdout);
    printf("Image voxels (nx,ny,nz):\n %ld %ld %ld\n",
	   nx,ny,nz);fflush(stdout);
    printf("Voxel sizes (dx,dy,dz):\n %12.4E %12.4E %12.4E\n",
	   dx,dy,dz);fflush(stdout);
    if(toRotate)
      printf("Image rotation (omega,theta,phi):\n %12.4E %12.4E %12.4E\n",
	     omega,theta,phi);
    else
      printf("No rotation applied.\n");
  }
  
  return 0;
}

image_spatialSampling::~image_spatialSampling(){
  if(F != nullptr)
    free(F);
  if(K != nullptr)
    free(K);
  F = nullptr;
  K = nullptr;
}

void image_spatialSampling::updateGeometry(const wrapper_geometry* geometry){

  //Check if the geometry is a DICOM
  const pen_dicomGeo* pDICOMgeo = dynamic_cast<const pen_dicomGeo*>(geometry);
  if(pDICOMgeo == nullptr){
    //Is not a DICOM based geometry
    translation[0] -= isocenter[0];
    translation[1] -= isocenter[1];
    translation[2] -= isocenter[2];        
  }
  else{
    //Is a DICOM based geometry

    if(!positionset){

      //Get DICOM
      const pen_dicom& dicom = pDICOMgeo->readDicom();
      
      long int nphx = 0;
      long int nphy = 0;
      long int nphz = 0;

      double dphx = 0.0;
      double dphy = 0.0;
      double dphz = 0.0;

      double Ophx = 0.0;
      double Ophy = 0.0;
      double Ophz = 0.0;
      
      //Store image dimensions
      nphx = dicom.getNX();
      nphy = dicom.getNY();
      nphz = dicom.getNZ();

      dphx = dicom.getDX();
      dphy = dicom.getDY();
      dphz = dicom.getDZ();

      Ophx = dicom.getOriginX();
      Ophy = dicom.getOriginY();
      Ophz = dicom.getOriginZ();

      translation[0] = Ox-Ophx-(isocenter[0]-isocenterph[0])-(dx/2.0-dphx/2.0)+imageCx;
      translation[1] = Oy-Ophy-(isocenter[1]-isocenterph[1])-(dy/2.0-dphy/2.0)+imageCy;
      translation[2] = Oz-Ophz-(isocenter[2]-isocenterph[2])-(dz/2.0-dphz/2.0)+imageCz;

      if(verbose > 1){
	printf(" Image source attached to a phantom image with:");
	printf("Phantom Origin (Ox,Oy,Oz):\n %12.4E %12.4E %12.4E\n",
	       Ophx,Ophy,Ophz);
	printf("Phantom voxels (nx,ny,nz):\n %ld %ld %ld\n",
	       nphx,nphy,nphz);
	printf("Phantom Voxel sizes (dx,dy,dz):\n %12.4E %12.4E %12.4E\n",
	       dphx,dphy,dphz);
	printf("Phantom Isocenter (Isox,Isoy,Isoz):\n %12.4E %12.4E %12.4E\n",
	       isocenterph[0],isocenterph[1],isocenterph[2]);
	printf("Final image center (x,y,z):\n %12.4E %12.4E %12.4E\n",
	       translation[0],translation[1],translation[2]);fflush(stdout);	
      }
      
    }
    else{
      translation[0] -= isocenter[0];
      translation[1] -= isocenter[1];
      translation[2] -= isocenter[2];    
    }    
  }
  
}

REGISTER_SAMPLER(image_spatialSampling,IMAGE)

#endif
