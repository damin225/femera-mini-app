#if VERB_MAX > 10
#include <iostream>
#endif
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <ctype.h>
#include <cstring>// std::memcpy
#include "femera.h"
//
int ElastIso3D::ElemLinear( Elem* ){ return 1; };//FIXME
int ElastIso3D::ElemJacobi( Elem* ){ return 1; };//FIXME
int ElastIso3D::ScatStiff( Elem* ){ return 1; };//FIXME
int ElastIso3D::BlocLinear( Elem* ,
  RESTRICT Phys::vals &, const RESTRICT Solv::vals & ){
  return 0;
  };
//
int ElastIso3D::Setup( Elem* E ){
  JacT  ( E );
  const uint elem_n = uint(E->elem_n);
  const uint jacs_n = uint(E->elip_jacs.size()/elem_n/ 10) ;
  const uint intp_n = uint(E->gaus_n);
  const uint conn_n = uint(E->elem_conn_n);
  this->tens_flop = uint(E->elem_n) * intp_n
    *( uint(E->elem_conn_n)* (54) + 27 );
  this->tens_band = elem_n *(
     sizeof(FLOAT_SOLV)*(3*conn_n*3+ jacs_n*10)// Main mem
    +sizeof(INT_MESH)*conn_n // Main mem ints
    +sizeof(FLOAT_PHYS)*(3*intp_n*conn_n +3+1 ) );// Stack (assumes read once)
  this->stif_flop = uint(E->elem_n)
    * 3*uint(E->elem_conn_n) *( 3*uint(E->elem_conn_n) );
  this->stif_band = uint(E->elem_n) * sizeof(FLOAT_PHYS)
    * 3*uint(E->elem_conn_n) *( 3*uint(E->elem_conn_n) +2);
  return 0;
};
int ElastIso3D::ElemLinear( Elem* E,
  FLOAT_SOLV* sys_f, const FLOAT_SOLV* sys_u ){
  //FIXME Clean up local variables.
  //const int De = 3;// Element Dimension
  const int Nd = 3;// Node (mesh) Dimension
  const int Nf = 3;// this->node_d DOF/node
  const int Nj = Nd*Nf+1;//FIXME wrong?
  const int Nc = E->elem_conn_n;// Number of nodes/element
  const int Ne = Nf*Nc;
  const INT_MESH elem_n =E->elem_n;
  const int intp_n = int(E->gaus_n);
  //
  INT_MESH e0=0, ee=elem_n;
  if(E->do_halo==true){ e0=0; ee=E->halo_elem_n;
  }else{ e0=E->halo_elem_n; ee=elem_n;};
#if VERB_MAX>11
  printf("DOF: %u, Elems:%u, IntPts:%u, Nodes/elem:%u\n",
    (uint)ndof,(uint)elem_n,(uint)intp_n,(uint)Nc );
#endif
  FLOAT_MESH jac[Nj];//, det;
  FLOAT_PHYS dw, G[Ne], u[Ne], f[Ne];
  FLOAT_PHYS H[Nd*Nf], S[Nd*Nf];
  //
  FLOAT_PHYS intp_shpg[intp_n*Ne];
  std::copy( &E->intp_shpg[0],
             &E->intp_shpg[intp_n*Ne], intp_shpg );
  FLOAT_PHYS wgt[intp_n];
  std::copy( &E->gaus_weig[0],
             &E->gaus_weig[intp_n], wgt );
  FLOAT_PHYS C[this->mtrl_matc.size()];
  std::copy( &this->mtrl_matc[0],
             &this->mtrl_matc[this->mtrl_matc.size()], C );
#if VERB_MAX>10
  printf( "Material [%u]:", (uint)mtrl_matc.size() );
  for(uint j=0;j<mtrl_matc.size();j++){
    //if(j%mesh_d==0){printf("\n");}
    printf("%+9.2e ",C[j]);
  }; printf("\n");
#endif
  const FLOAT_MESH* RESTRICT Ejacs = &E->elip_jacs[0];
  const FLOAT_SOLV* RESTRICT sysu  = &sys_u[0];
        FLOAT_SOLV* RESTRICT sysf  = &sys_f[0];
  if(e0<ee){
    std::memcpy( &jac , &Ejacs[Nj*e0], sizeof(FLOAT_MESH)*Nj);
    for (int i=0; i<Nc; i++){
      std::memcpy( & u[Nf*i],& sysu[E->elem_conn[Nc*e0+i]*Nf],
        sizeof(FLOAT_SOLV)*Nf ); };
  };
  for(INT_MESH ie=e0;ie<ee;ie++){
    for (int i=0; i<Nc; i++){
      std::memcpy( & f[Nf*i],& sysf[E->elem_conn[Nc*ie+i]*3],
        sizeof(FLOAT_SOLV)*Nf ); };
    for(int ip=0; ip<intp_n; ip++){
      //G = MatMul3x3xN( jac,shg );
      //H = MatMul3xNx3T( G,u );// [H] Small deformation tensor
      for(int i=0; i< 9 ; i++){ H[i]=0.0; };
      for(int i=0; i<Nc; i++){
        for(int k=0; k<Nf ; k++){ G[Nf* i+k ]=0.0;
          for(int j=0; j<Nd ; j++){
            G[(Nf* i+k) ] += intp_shpg[ip*Ne+ Nd* i+j ] * jac[Nd* j+k ];
          };
          for(int j=0; j<Nf ; j++){
            H[(Nf* k+j) ] += G[(Nf* i+k) ] * u[Nf* i+j ];
          };
        };// 36*N FMA FLOP
};//------------------------------------------------ N*3*6*2 = 36*N FLOP
#if VERB_MAX>10
      printf( "Small Strains (Elem: %i):", ie );
      for(int j=0;j<H.size();j++){
        if(j%mesh_d==0){printf("\n");}
        printf("%+9.2e ",H[j]);
      }; printf("\n");
#endif
      dw = jac[9] * wgt[ip];
      if(ip==(intp_n-1)){ if((ie+1)<ee){// Fetch stuff for the next iteration
        for (int i=0; i<Nc; i++){
          std::memcpy(& u[Nf*i],& sysu[E->elem_conn[Nc*(ie+1)+i]*Nf],
            sizeof(FLOAT_SOLV)*Nf ); };
        std::memcpy( &jac, &Ejacs[Nj*(ie+1)], sizeof(FLOAT_MESH)*Nj);
      }; };
      //
      S[0]=(C[0]* H[0] + C[1]* H[4] + C[1]* H[8])*dw;//Sxx
      S[4]=(C[1]* H[0] + C[0]* H[4] + C[1]* H[8])*dw;//Syy
      S[8]=(C[1]* H[0] + C[1]* H[4] + C[0]* H[8])*dw;//Szz
      //
      S[1]=( H[1] + H[3] )*C[2]*dw;// S[3]= S[1];//Sxy Syx
      S[5]=( H[5] + H[7] )*C[2]*dw;// S[7]= S[5];//Syz Szy
      S[2]=( H[2] + H[6] )*C[2]*dw;// S[6]= S[2];//Sxz Szx
      S[3]=S[1]; S[7]=S[5]; S[6]=S[2];
      //------------------------------------------------------- 18+9 = 27 FLOP
      for(int i=0; i<Nc; i++){
        for(int k=0; k<Nf; k++){
          for(int j=0; j<Nf; j++){
            f[(Nf* i+k) ] += G[(Nf* i+j) ] * S[(Nf* j+k) ];// 18*N FMA FLOP
      };};};//---------------------------------------------- N*3*6 = 18*N FLOP
#if VERB_MAX>10
      printf( "f:");
      for(int j=0;j<Ne;j++){
        if(j%ndof==0){printf("\n");}
        printf("%+9.2e ",f[j]);
      }; printf("\n");
#endif
    };//end intp loop
    for (uint i=0; i<uint(Nc); i++){
      const INT_MESH n=E->elem_conn[Nc*ie+i];
      std::memcpy(& sysf[n*3],& f[Nf*i], sizeof(FLOAT_SOLV)*Nf );
#if 0
      if( n >=my_node_start ){
        for(uint j=0;j<3;j++){
          this->part_sum1+= f[Nf* i+j ] * sysu[Nf* n+j ];
          //FIXME u already contains next elem sys_u
          //this->part_sum1+= f[Nf* i+j ] * u[Nf* i+j];
        };
      };
#endif
    };
  };//end elem loop
  return 0;
  };
int ElastIso3D::ElemJacobi(Elem* E, FLOAT_SOLV* sys_d ){
  const uint ndof   = 3;//this->node_d
  const uint elem_n = E->elem_n;
  const uint  Nc = E->elem_conn_n;
  const uint  Nj = 10,d2=9;
  const uint  Ne = uint(ndof*Nc);
  const uint intp_n = E->gaus_n;
  //
  FLOAT_PHYS det;
  RESTRICT Phys::vals elem_diag(Ne);
  //RESTRICT Phys::vals B(Ne*6);// 6 rows, Ne cols
  FLOAT_PHYS B[Ne*6];
  FLOAT_PHYS G[Ne],jac[Nj];//,elem_diag[Ne];// 6 rows, Ne cols
  for(uint j=0; j<(Ne*6); j++){ B[j]=0.0; };
  const FLOAT_PHYS D[]={
    mtrl_matc[0],mtrl_matc[1],mtrl_matc[1],0.0,0.0,0.0,
    mtrl_matc[1],mtrl_matc[0],mtrl_matc[1],0.0,0.0,0.0,
    mtrl_matc[1],mtrl_matc[1],mtrl_matc[0],0.0,0.0,0.0,
    0.0,0.0,0.0,mtrl_matc[2]*2.0,0.0,0.0,
    0.0,0.0,0.0,0.0,mtrl_matc[2]*2.0,0.0,
    0.0,0.0,0.0,0.0,0.0,mtrl_matc[2]*2.0};
  //elem_inout=0.0;
  for(uint ie=0;ie<elem_n;ie++){
    uint ij=Nj*ie;//FIXME only good for tets
    std::copy( &E->elip_jacs[ij],
               &E->elip_jacs[ij+Nj], jac ); det=jac[d2];
    for(uint ip=0;ip<intp_n;ip++){
      uint ig=ip*Ne;
      for(uint i=0;i<Ne;i++){ G[i]=0.0; };
      for(uint k=0;k<Nc;k++){
      for(uint i=0;i<3;i++){
      for(uint j=0;j<3;j++){
        G[Nc* i+k] += jac[3* j+i] * E->intp_shpg[ig+3* k+j]; }; }; };
      #if VERB_MAX>10
      printf( "Jacobian Inverse & Determinant:");
      for(uint j=0;j<d2;j++){
        if(j%3==0){printf("\n");}
        printf("%+9.2e",jac[j]);
      }; printf(" det:%+9.2e\n",det);
      #endif
      // xx yy zz
      for(uint j=0; j<Nc; j++){
        B[Ne*0 + 0+j*ndof] = G[Nc*0+j];
        B[Ne*1 + 1+j*ndof] = G[Nc*1+j];
        B[Ne*2 + 2+j*ndof] = G[Nc*2+j];
      // xy yx
        B[Ne*3 + 0+j*ndof] = G[Nc*1+j];
        B[Ne*3 + 1+j*ndof] = G[Nc*0+j];
      // yz zy
        B[Ne*4 + 1+j*ndof] = G[Nc*2+j];
        B[Ne*4 + 2+j*ndof] = G[Nc*1+j];
      // xz zx
        B[Ne*5 + 0+j*ndof] = G[Nc*2+j];
        B[Ne*5 + 2+j*ndof] = G[Nc*0+j];
      };
      #if VERB_MAX>10
      printf( "[B]:");
      for(uint j=0;j<B.size();j++){
        if(j%Ne==0){printf("\n");}
        printf("%+9.2e ",B[j]);
      }; printf("\n");
      #endif
      FLOAT_PHYS w = det * E->gaus_weig[ip];
      for(uint i=0; i<Ne; i++){
      for(uint k=0; k<6 ; k++){
      for(uint j=0; j<6 ; j++){
        elem_diag[i]+=(B[Ne*j + i] * D[6*j + k] * B[Ne*k + i])*w; };
      };};
    };//end intp loop
    for (uint i=0; i<Nc; i++){
      for(uint j=0; j<3; j++){
        sys_d[E->elem_conn[Nc*ie+i]*3+j] += elem_diag[3*i+j];
      }; };
    elem_diag=0.0;
  };
  return 0;
};
int ElastIso3D::ElemRowSumAbs(Elem* E, FLOAT_SOLV* sys_d ){
  const uint ndof   = 3;//this->node_d
  const uint elem_n = E->elem_n;
  const uint  Nc = E->elem_conn_n;
  const uint  Nj = 10,d2=9;
  const uint  Ne = uint(ndof*Nc);
  const uint intp_n = E->gaus_n;
  //
  FLOAT_PHYS det;
  RESTRICT Phys::vals elem_sum(Ne);
  RESTRICT Phys::vals K(Ne*Ne);
  FLOAT_PHYS B[Ne*6];//6 rows, Ne cols
  FLOAT_PHYS G[Ne],jac[Nj];
  for(uint j=0; j<(Ne*6); j++){ B[j]=0.0; };
  const FLOAT_PHYS D[]={
    mtrl_matc[0],mtrl_matc[1],mtrl_matc[1],0.0,0.0,0.0,
    mtrl_matc[1],mtrl_matc[0],mtrl_matc[1],0.0,0.0,0.0,
    mtrl_matc[1],mtrl_matc[1],mtrl_matc[0],0.0,0.0,0.0,
    0.0,0.0,0.0,mtrl_matc[2]*2.0,0.0,0.0,
    0.0,0.0,0.0,0.0,mtrl_matc[2]*2.0,0.0,
    0.0,0.0,0.0,0.0,0.0,mtrl_matc[2]*2.0};
  for(uint ie=0;ie<elem_n;ie++){
    uint ij=Nj*ie;//FIXME only good for tets
    std::copy( &E->elip_jacs[ij],
               &E->elip_jacs[ij+Nj], jac ); det=jac[d2];
    for(uint ip=0;ip<intp_n;ip++){
      uint ig=ip*Ne;
      for(uint i=0;i<Ne;i++){ G[i]=0.0; };
      for(uint k=0;k<Nc;k++){
      for(uint i=0;i<3;i++){
      for(uint j=0;j<3;j++){
        G[3* i+k] += jac[3* j+i] * E->intp_shpg[ig+3* k+j]; }; }; };
      #if VERB_MAX>10
      printf( "Jacobian Inverse & Determinant:");
      for(uint j=0;j<d2;j++){
        if(j%3==0){printf("\n");}
        printf("%+9.2e",jac[j]);
      }; printf(" det:%+9.2e\n",det);
      #endif
      // xx yy zz
      for(uint j=0; j<Nc; j++){
        B[Ne*0 + 0+j*ndof] = G[Nc*0+j];
        B[Ne*1 + 1+j*ndof] = G[Nc*1+j];
        B[Ne*2 + 2+j*ndof] = G[Nc*2+j];
      // xy yx
        B[Ne*3 + 0+j*ndof] = G[Nc*1+j];
        B[Ne*3 + 1+j*ndof] = G[Nc*0+j];
      // yz zy
        B[Ne*4 + 1+j*ndof] = G[Nc*2+j];
        B[Ne*4 + 2+j*ndof] = G[Nc*1+j];
      // xz zx
        B[Ne*5 + 0+j*ndof] = G[Nc*2+j];
        B[Ne*5 + 2+j*ndof] = G[Nc*0+j];
      };
      #if VERB_MAX>10
      printf( "[B]:");
      for(uint j=0;j<B.size();j++){
        if(j%Ne==0){printf("\n");}
        printf("%+9.2e ",B[j]);
      }; printf("\n");
      #endif
      FLOAT_PHYS w = det * E->gaus_weig[ip];
      for(uint i=0; i<Ne; i++){
      for(uint l=0; l<Ne; l++){
      for(uint j=0; j<6 ; j++){
      for(uint k=0; k<6 ; k++){
        K[Ne*i+l]+= B[Ne*j + i] * D[6*j + k] * B[Ne*k + l]*w;
      };};};};
    };//end intp loop
    for (uint i=0; i<Ne; i++){
      for(uint j=0; j<Ne; j++){
        elem_sum[i] += std::abs(K[Ne*i+j]);
      };};
    for (uint i=0; i<Nc; i++){
      for(uint j=0; j<3; j++){
        sys_d[E->elem_conn[Nc*ie+i]*3+j] += elem_sum[3*i+j];
      };};
    K=0.0; elem_sum=0.0;
  };
  return 0;
};
int ElastIso3D::ElemStrain( Elem* E,FLOAT_SOLV* sys_f ){
  //FIXME Clean up local variables.
  const uint ndof= 3;//this->node_d
  const uint  Nj =10;//,d2=9;//mesh_d*mesh_d;
  const INT_MESH elem_n = E->elem_n;
  const uint intp_n = uint(E->gaus_n);
  const uint     Nc = E->elem_conn_n;// Number of Nodes/Element
  const uint     Ne = ndof*Nc;
  //FLOAT_PHYS det;
  INT_MESH   conn[Nc];
  FLOAT_MESH jac[Nj];
  FLOAT_PHYS dw, G[Ne], f[Ne];
  FLOAT_PHYS H[9], S[9];
  //
  for(uint i=0; i< 9 ; i++){ H[i]=0.0; };
  H[0]=1.0; H[4]=1.0; H[8]=1.0;// unit pressure
  //
  FLOAT_PHYS intp_shpg[intp_n*Ne];
  std::copy( &E->intp_shpg[0],
             &E->intp_shpg[intp_n*Ne], intp_shpg );
  FLOAT_PHYS wgt[intp_n];
  std::copy( &E->gaus_weig[0],
             &E->gaus_weig[intp_n], wgt );
  FLOAT_PHYS C[this->mtrl_matc.size()];
  std::copy( &this->mtrl_matc[0],
             &this->mtrl_matc[this->mtrl_matc.size()], C );
  const auto Econn = &E->elem_conn[0];
  const auto Ejacs = &E->elip_jacs[0];
  //
  for(INT_MESH ie=0;ie<elem_n;ie++){
    std::memcpy( &conn, &Econn[Nc*ie], sizeof(  INT_MESH)*Nc);
    std::memcpy( &jac , &Ejacs[Nj*ie], sizeof(FLOAT_MESH)*Nj);
    //
    for(uint i=0;i<(Ne);i++){ f[i]=0.0; };
    for(uint ip=0; ip<intp_n; ip++){
      //G = MatMul3x3xN( jac,shg );
      //H = MatMul3xNx3T( G,u );// [H] Small deformation tensor
      for(uint k=0; k<Nc; k++){
        for(uint i=0; i<3 ; i++){ G[3* k+i ]=0.0;
          for(uint j=0; j<3 ; j++){
            G[(3* k+i) ] += jac[3* j+i ] * intp_shpg[ip*Ne+ 3* k+j ];
          };
        };
      };//------------------------------------------------- N*3*6*2 = 36*N FLOP
#if VERB_MAX>10
      printf( "Small Strains (Elem: %i):", ie );
      for(uint j=0;j<9;j++){
        if(j%mesh_d==0){printf("\n");}
        printf("%+9.2e ",H[j]);
      }; printf("\n");
#endif
      //det=jac[9 +Nj*l]; FLOAT_PHYS w = det * wgt[ip];
      dw = jac[9] * wgt[ip];
      //
      S[0]=(C[0]* H[0] + C[1]* H[4] + C[1]* H[8])*dw;//Sxx
      S[4]=(C[1]* H[0] + C[0]* H[4] + C[1]* H[8])*dw;//Syy
      S[8]=(C[1]* H[0] + C[1]* H[4] + C[0]* H[8])*dw;//Szz
      //
      S[1]=( H[1] + H[3] )*C[2]*dw;// S[3]= S[1];//Sxy Syx
      S[5]=( H[5] + H[7] )*C[2]*dw;// S[7]= S[5];//Syz Szy
      S[2]=( H[2] + H[6] )*C[2]*dw;// S[6]= S[2];//Sxz Szx
      S[3]=S[1]; S[7]=S[5]; S[6]=S[2];
      //------------------------------------------------------- 18+9 = 27 FLOP
      for(uint i=0; i<Nc; i++){
        for(uint k=0; k<3; k++){
          for(uint j=0; j<3; j++){
            f[(3* i+k) ] += G[(3* i+j) ] * S[(3* k+j) ];
      };};};//---------------------------------------------- N*3*6 = 18*N FLOP
#if VERB_MAX>10
      printf( "f:");
      for(uint j=0;j<Ne;j++){
        if(j%ndof==0){printf("\n");}
        printf("%+9.2e ",f[j]);
      }; printf("\n");
#endif
    };//end intp loop
    for (uint i=0; i<Nc; i++){
      for(uint j=0; j<3; j++){
        //sys_f[3*conn[i]+j] += f[(3*i+j)];
        sys_f[4*conn[i]+j] += std::abs( f[(3*i+j)] );
    }; };//--------------------------------------------------- N*3 =  3*N FLOP
  };//end elem loop
  return 0;
  };
#if 0
int ElastIso3D::ReadPartFMR( const char* fname, bool is_bin ){
  //FIXME This is not used. It's done in Elem::ReadPartFMR...
  std::string s; if(is_bin){ s="binary";}else{s="ASCII";}
  if(is_bin){
    std::cout << "ERROR Could not open "<< fname << " for reading." <<'\n'
      << "ERROR Femera (fmr) "<< s <<" format not yet supported." <<'\n';
    return 1;
  }
  std::string fmrstring;
  std::ifstream fmrfile(fname);
  while( fmrfile >> fmrstring ){
    if(fmrstring=="$ElasticProperties"){//FIXME Deprecated
      int s=0; fmrfile >> s;
      mtrl_prop.resize(s);
      for(int i=0; i<s; i++){ fmrfile >> mtrl_prop[i]; }
      //this->MtrlProp2MatC();
      s=0; fmrfile >> s;
      if(s>0){
        mtrl_dirs.resize(s);
      for(int i=0; i<s; i++){ fmrfile >> mtrl_dirs[i]; mtrl_dirs[i]*=(PI/180.0) ;}
      }
    }
    if(fmrstring=="$Orientation"){// Material orientation (radians)
      int s=0; fmrfile >> s;
      if(s>0){
        mtrl_dirs.resize(s);
        for(int i=0; i<s; i++){ fmrfile >> mtrl_dirs[i]; mtrl_dirs[i]*=(PI/180.0) ;}
      }
    }
    //FIXME This parsing requires properties in a specific order
    auto tprop = mtrl_prop; auto tsz=tprop.size();
    if(fmrstring=="$Elastic"){// Elastic Constants
      int s=0; fmrfile >> s;
      mtrl_prop.resize(tsz+s);
      mtrl_prop[std::slice(tsz,tsz+s,1)] = tprop;
      for(int i=0; i<s; i++){ fmrfile >> mtrl_prop[i+tprop.size()]; }
    }
    if(fmrstring=="$ThermalExpansion"){// Thermal expansion
      int s=0; fmrfile >> s;
      mtrl_prop.resize(s + tprop.size());
      mtrl_prop[std::slice(tsz,tsz+s,1)] = tprop;
      for(int i=0; i<s; i++){ fmrfile >> mtrl_prop[i+tprop.size()]; }
    }
    if(fmrstring=="$ThermalConductivity"){// Thermal conductivity
      int s=0; fmrfile >> s;
      mtrl_prop.resize(s + tprop.size());
      mtrl_prop[std::slice(tsz,tsz+s,1)] = tprop;
      for(int i=0; i<s; i++){ fmrfile >> mtrl_prop[i+tprop.size()]; }
    }
  }
  return 0;
}
int ElastIso3D::SavePartFMR( const char* fname, bool is_bin ){
  std::string s; if(is_bin){ s="binary";}else{s="ASCII";};
  if(is_bin){
    std::cout << "ERROR Could not append "<< fname << "." <<'\n'
      << "ERROR Femera (fmr) "<< s <<" format not yet supported." <<'\n';
    return 1;
  };
  std::ofstream fmrfile;
  fmrfile.open(fname, std::ios_base::app);
  //
  fmrfile << "$ElasticProperties" <<'\n';
  fmrfile << mtrl_prop.size();
  for(uint i=0;i<mtrl_prop.size();i++){ fmrfile <<" "<< mtrl_prop[i]; };
  fmrfile << '\n';
  if(mtrl_dirs.size()>0){
    fmrfile << mtrl_dirs.size();
    for(uint i=0;i<mtrl_dirs.size();i++){ fmrfile <<" "<< mtrl_dirs[i]; };
  }; fmrfile <<'\n';
  return 0;
};
#endif