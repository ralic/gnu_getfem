clear pde;
gf_workspace('clear all');

NX = 10
m  = gf_mesh('triangles grid',[0:1/NX:1],[0:1/NX:1]);
//gf_mesh_set(m,'transform', [.3 .8; .8 -.2]);
//m = gf_mesh('pt2d', [0 0; 1 0; 0 1]', [1 2 3]');

// create a mesh_fem of for a field of dimension 1 (i.e. a scalar field)
mf   = gf_mesh_fem(m,1);
mfl  = gf_mesh_fem(m,1);
mflg = gf_mesh_fem(m,1);
mflh = gf_mesh_fem(m,1);

// assign the Q2 fem to all convexes of the mesh_fem,
//gf_mesh_fem_set(mf,'fem',gf_fem('FEM_PK(2,3)'));
//gf_mesh_fem_set(mf,'fem',gf_fem('FEM_ARGYRIS'));
gf_mesh_fem_set(mf,'fem',gf_fem('FEM_HCT_TRIANGLE'));
//gf_mesh_fem_set(mf,'fem',gf_fem('FEM_HERMITE(2)'));
gf_mesh_fem_set(mfl,'fem',gf_fem('FEM_PK(2,5)'));
gf_mesh_fem_set(mflg,'fem',gf_fem('FEM_PK_DISCONTINUOUS(2,4)'));
gf_mesh_fem_set(mflh,'fem',gf_fem('FEM_PK_DISCONTINUOUS(2,3)'));

// an exact integration will be used
//mim = gf_mesh_im(m, gf_integ('IM_TRIANGLE(13)'));
mim = gf_mesh_im(m, gf_integ('IM_HCT_COMPOSITE(IM_TRIANGLE(13))'));

// detect the border of the mesh
border = gf_mesh_get(m,'outer faces');

// mark it as boundary #1
gf_mesh_set(m, 'boundary', 1, border);
gf_plot_mesh(m, 'regions', [1]); // the boundary edges appears in red
pause(1);
                                                  // exact solution
if 0 then
  // setup a pde structure for gf_solve
  pde.type   = 'laplacian'; // YC:
  pde.lambda = list(1);  // note the use of braces
  pde.mim    = mim;
  pde.mf_u   = mf;       // this does not copy whole objects, just their handles
  pde.mf_d   = mfl;
  expr_u     = 'y.*(y-1).*x.*(x-1)+x.^5/10';
  expr_f     = '-(2*(x.^2+y.^2)-2*x-2*y+20*x.^3/10)';
  pde.F      = list( expr_f );         // the volumic source
  pde.bound(1).type = 'Dirichlet';
  pde.bound(1).R    = list( expr_u );  // we force the value of the solution on the boundary
  U      = gf_solve(pde);
  Uexact = gf_mesh_fem_get(mfl,'eval', list( expr_u )); // interpolate the
else
  expr_u = 'y.^5';
  Uexact = gf_mesh_fem_get(mfl,'eval', list( expr_u )); // interpolate the
  M = gf_asm('mass matrix', mim, mf, mf);
  F = gf_asm('volumic source', mim, mf, mfl, Uexact);
  U = (M\F)';
end
						
Ul    = gf_compute(mf,U,'interpolate on', mfl);
DUl   = gf_compute(mfl, Ul, 'gradient',mflg);
D2Ul  = gf_compute(mflg, DUl, 'gradient',mflh);
D2Ul2 = gf_compute(mfl,Ul, 'hessian',mflh);
nref  = 4

subplot(2,2,1); 
gf_plot(mfl,Ul,'mesh','on','refine',nref,'contour',.01:.01:.1); 
//colorbar;
title('computed solution');

subplot(2,2,2); 
gf_plot(mfl,Ul-Uexact,'mesh','on','refine',nref); 
//colorbar;
title('difference with exact solution');

subplot(2,2,3); 
gf_plot(mflg,DUl(1,:),'mesh','on', 'refine', nref); 
//colorbar;
title('gradx');

subplot(2,2,4); 
gf_plot(mflg,DUl(2,:),'mesh','on', 'refine', nref); 
//colorbar;
title('grady');

disp(sprintf('H1 norm of error: %g', gf_compute(mfl,Ul-Uexact,'H1 norm',mim)));
