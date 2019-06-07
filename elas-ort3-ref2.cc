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
int ElastOrtho3D::Setup( Elem* E ){
  JacRot( E );
  JacT  ( E );
  const uint jacs_n = E->elip_jacs.size()/E->elem_n/ 10 ;
  const uint intp_n = E->gaus_n;
  this->tens_flop = uint(E->elem_n) * intp_n
    *( uint(E->elem_conn_n)* (36+18+3) + 2*54 + 27 );
  this->tens_band = uint(E->elem_n) *(
     sizeof(FLOAT_PHYS)*(3*uint(E->elem_conn_n)*3+ jacs_n*10)
    +sizeof(INT_MESH)*uint(E->elem_conn_n) );
  this->stif_flop = uint(E->elem_n)
    * 3*uint(E->elem_conn_n) *( 3*uint(E->elem_conn_n) );
  this->stif_band = uint(E->elem_n) *(
    sizeof(FLOAT_PHYS)* 3*uint(E->elem_conn_n) *( 3*uint(E->elem_conn_n) -1+2)
    +sizeof(INT_MESH) *uint(E->elem_conn_n) );
  return 0;
};
int ElastOrtho3D::ElemLinear( Elem* E,
  FLOAT_SOLV *sys_f, const FLOAT_SOLV* sys_u ){
  //FIXME Cleanup local variables.
  const uint ndof   = 3;//this->ndof_n
  //const int mesh_d = 3;//E->elem_d;
  const uint  Nj =10;//,d2=9;//mesh_d*mesh_d;
  //
  const INT_MESH elem_n = E->elem_n;
  const uint     Nc = E->elem_conn_n;// Number of Nodes/Element
  const uint     Ne = ndof*Nc;
  const uint intp_n = uint(E->gaus_n);
  //
  INT_MESH e0=0, ee=elem_n;
  if(E->do_halo==true){ e0=0; ee=E->halo_elem_n;
  }else{ e0=E->halo_elem_n; ee=elem_n; };
  //
#if VERB_MAX>11
  printf("Dim: %i, Elems:%i, IntPts:%i, Nodes/elem:%i\n",
    (int)mesh_d,(int)elem_n,(int)intp_n,(int)Nc);
#endif
  INT_MESH   conn[Nc];
  FLOAT_MESH jac[Nj];
  FLOAT_PHYS dw, G[Ne], u[Ne],f[Ne];
  //FLOAT_PHYS det,
  FLOAT_PHYS H[9], S[9], A[9];//, B[9];
  //
  FLOAT_PHYS intp_shpg[intp_n*Ne];
  std::copy( &E->intp_shpg[0],// local copy
             &E->intp_shpg[intp_n*Ne], intp_shpg );
  FLOAT_PHYS wgt[intp_n];
  std::copy( &E->gaus_weig[0],
             &E->gaus_weig[intp_n], wgt );
  FLOAT_PHYS C[this->mtrl_matc.size()];
  std::copy( &this->mtrl_matc[0],
             &this->mtrl_matc[this->mtrl_matc.size()], C );
  const FLOAT_PHYS R[9] = {
    mtrl_rotc[0],mtrl_rotc[1],mtrl_rotc[2],
    mtrl_rotc[3],mtrl_rotc[4],mtrl_rotc[5],
    mtrl_rotc[6],mtrl_rotc[7],mtrl_rotc[8]};
#if VERB_MAX>10
  printf( "Material [%u]:", (uint)mtrl_matc.size() );
  for(uint j=0;j<mtrl_matc.size();j++){
    //if(j%mesh_d==0){printf("\n");}
    printf("%+9.2e ",C[j]);
  }; printf("\n");
#endif
  const auto Econn = &E->elem_conn[0];
  const auto Ejacs = &E->elip_jacs[0];
  const auto sysu0 = &sys_u[0];
  for(INT_MESH ie=e0;ie<ee;ie++){
    std::memcpy( &conn, &Econn[Nc*ie], sizeof(  INT_MESH)*Nc);
    std::memcpy( &jac , &Ejacs[Nj*ie], sizeof(FLOAT_MESH)*Nj);
    //std::copy( &Econn[Nc*ie],
    //           &Econn[Nc*ie+Nc], conn );
    //std::copy( &Ejacs[Nj*ie],
    //           &Ejacs[Nj*ie+Nj], jac );// det=jac[9];
    for (uint i=0; i<(Nc); i++){
      //std::memcpy( &    u[ndof*i],
      std::memcpy( &    u[ndof*i],
                   //&sys_u[Econn[Nc*ie+i]*ndof], sizeof(FLOAT_SOLV)*ndof ); };
                   &sysu0[conn[i]*ndof], sizeof(FLOAT_SOLV)*ndof ); };
    for(uint i=0;i<(Ne);i++){ f[i]=0.0; };
    for(uint ip=0; ip<intp_n; ip++){
      //G = MatMul3x3xN( jac,shg );
      //A = MatMul3xNx3T( G,u );
      for(uint i=0; i< 9 ; i++){ A[i]=0.0;};// H[i]=0.0; B[i]=0.0; };
      //for(uint i=0; i<(Ne) ; i++){ G[i]=0.0; };
//#pragma omp simd
      for(uint k=0; k<Nc; k++){
        for(uint i=0; i<3 ; i++){ G[3* k+i ]=0.0;
          for(uint j=0; j<3 ; j++){
            G[(3* k+i) ] += jac[3* j+i ] * intp_shpg[ip*Ne+ 3* k+j ];
          };
          for(uint j=0; j<3 ; j++){
            A[(3* i+j) ] += G[(3* k+i) ] * u[ndof* k+j ];
          };
        };
      };//------------------------------------------------- N*3*6*2 = 36*N FLOP
#if VERB_MAX>10
      printf( "Small Strains (Elem: %i):", ie );
      for(uint j=0;j<HH.size();j++){
        if(j%mesh_d==0){printf("\n");}
        printf("%+9.2e ",H[j]);
      }; printf("\n");
#endif
      // [H] Small deformation tensor
      // [H][RT] : matmul3x3x3T
//#pragma omp simd
      for(uint i=0; i<3; i++){
        for(uint k=0; k<3; k++){ H[3* i+k ]=0.0;
          for(uint j=0; j<3; j++){
            H[(3* i+k) ] += A[(3* i+j)] * R[3* k+j ];
      };};};//---------------------------------------------- 27*2 =      54 FLOP
      //det=jac[9 +Nj*l]; FLOAT_PHYS w = det * wgt[ip];
      dw = jac[9] * wgt[ip];
      //
      S[0]=(C[0]* H[0] + C[3]* H[4] + C[5]* H[8])*dw;//Sxx
      S[4]=(C[3]* H[0] + C[1]* H[4] + C[4]* H[8])*dw;//Syy
      S[8]=(C[5]* H[0] + C[4]* H[4] + C[2]* H[8])*dw;//Szz
      //
      S[1]=( H[1] + H[3] )*C[6]*dw;// S[3]= S[1];//Sxy Syx
      S[5]=( H[5] + H[7] )*C[7]*dw;// S[7]= S[5];//Syz Szy
      S[2]=( H[2] + H[6] )*C[8]*dw;// S[6]= S[2];//Sxz Szx
      S[3]=S[1]; S[7]=S[5]; S[6]=S[2];
#if VERB_MAX>10
      printf( "Stress (Natural Coords):");
      for(uint j=0;j<9;j++){
        if(j%3==0){printf("\n");}
        printf("%+9.2e ",S[j]);
      }; printf("\n");
#endif
      //--------------------------------------------------------- 18+9= 27 FLOP
      // [S][R] : matmul3x3x3, R is transposed
      //for(int i=0; i<9; i++){ A[i]=0.0; };
//#pragma omp simd
      for(uint i=0; i<3; i++){
        //for(int k=0; k<3; k++){ A[3* i+k ]=0.0;
        for(uint k=0; k<3; k++){ A[3* k+i ]=0.0;
          for(uint j=0; j<3; j++){
            //A[3* i+k ] += S[3* i+j ] * R[3* j+k ];
            A[(3* k+i) ] += S[(3* i+j) ] * R[3* j+k ];// A is transposed
      };};};//-------------------------------------------------- 27*2 = 54 FLOP
      //NOTE [A] is not symmetric Cauchy stress.
      //NOTE Cauchy stress is ( A + AT ) /2
#if VERB_MAX>10
      printf( "Rotated Stress (Global Coords):");
      for(uint j=0;j<9;j++){
        if(j%3==0){printf("\n");}
        printf("%+9.2e ",S[j]);
      }; printf("\n");
#endif
//#pragma omp simd
      for(uint i=0; i<Nc; i++){
        for(uint k=0; k<3; k++){
          for(uint j=0; j<3; j++){
            f[(3* i+k) ] += G[(3* i+j) ] * A[(3* k+j) ];
      };};};//---------------------------------------------- N*3*6 = 18*N FLOP
      // This is way slower:
      //for(uint i=0; i<Nc; i++){
      //  for(uint k=0; k<3 ; k++){
      //    for(uint j=0; j<3 ; j++){
      //    for(uint l=0; l<3 ; l++){
      //      f[3* i+l ] += G[3* i+j ] * S[3* j+k ] * R[3* k+l ];
      //};};};};
#if VERB_MAX>10
      printf( "ff:");
      for(uint j=0;j<Ne;j++){
        if(j%mesh_d==0){printf("\n");}
        printf("%+9.2e ",f[j]);
      }; printf("\n");
#endif
    };//end intp loop
    for (uint i=0; i<Nc; i++){
      for(uint j=0; j<3; j++){
        //sys_f[3*Econn[Nc*ie+i]+j] += f[(3*i+j)];
        sys_f[3*conn[i]+j] += f[(3*i+j)];
    }; };//--------------------------------------------------- N*3 =  3*N FLOP
  };//end elem loop
  return 0;
};