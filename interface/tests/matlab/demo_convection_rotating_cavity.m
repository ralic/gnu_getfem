% Copyright (C) 2009-2017 Yves Renard.
%
% This file is a part of GetFEM++
%
% GetFEM++  is  free software;  you  can  redistribute  it  and/or modify it
% under  the  terms  of the  GNU  Lesser General Public License as published
% by  the  Free Software Foundation;  either version 3 of the License,  or
% (at your option) any later version along with the GCC Runtime Library
% Exception either version 3.1 or (at your option) any later version.
% This program  is  distributed  in  the  hope  that it will be useful,  but
% WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
% or  FITNESS  FOR  A PARTICULAR PURPOSE.  See the GNU Lesser General Public
% License and GCC Runtime Library Exception for more details.
% You  should  have received a copy of the GNU Lesser General Public License
% along  with  this program;  if not, write to the Free Software Foundation,
% Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA.
%
% The transport equation into the unit square (rotating cavity)
%



% trace on;
gf_workspace('clear all');

K0 = 2;  % degree for u
K1 = 2;  % degree for v
NX = 10;
scheme = 1; % 0 = Implicit Euler
            % 1 = midpoint

%m = gf_mesh('cartesian', 0:1/NX:1, 0:1/NX:1);
m = gf_mesh('triangles grid', 0:1/NX:1, 0:1/NX:1);


border = gf_mesh_get(m,'outer faces');
% normals = gf_mesh_get(m, 'normal of faces', border);
% dirichlet_boundary1=border(:,find(normals(2, :) < -0.5));
% dirichlet_boundary2=border(:,find(normals(1, :) < -0.5));
dirichlet_boundary = border;
GAMMAD = 1;
gf_mesh_set(m, 'region', GAMMAD, dirichlet_boundary);
% gf_plot_mesh(m, 'regions', [GAMMAD]); % the boundary edges appears in red
% pause;

%m=gf_mesh('import','structured','GT="GT_QK(2,1)";SIZES=[1,1];NOISED=1;NSUBDIV=[1,1];')

mf_u = gf_mesh_fem(m,1);
mf_v = gf_mesh_fem(m,1);
mf_d = gf_mesh_fem(m,2);
gf_mesh_fem_set(mf_u,'fem',gf_fem(sprintf('FEM_PK(2,%d)', K0)));
gf_mesh_fem_set(mf_v,'fem',gf_fem(sprintf('FEM_PK(2,%d)', K1)));
gf_mesh_fem_set(mf_d,'fem',gf_fem(sprintf('FEM_PK(2,%d)', K0)));
nbdofu = gf_mesh_fem_get(mf_u, 'nbdof');
nbdofv = gf_mesh_fem_get(mf_v, 'nbdof');


F = (gf_mesh_fem_get(mf_d, 'eval', {'0.5-y', 'x-0.5'}))';

% Integration which will be used
% mim = gf_mesh_im(m, gf_integ('IM_GAUSS_PARALLELEPIPED(2,4)'));
mim = gf_mesh_im(m, gf_integ('IM_TRIANGLE(6)'));



% Matrices
K = gf_asm('volumic', 'a=data(#2); M(#1,#1)+=comp(Grad(#1).Base(#1).vBase(#2))(:,i,:,j,i).a(j)', mim, mf_u, mf_d, F);
C = gf_asm('mass matrix', mim, mf_v);
B = gf_asm('mass matrix', mim, mf_v, mf_u);

dirichlet_dof = gf_mesh_fem_get(mf_u, 'dof on region', GAMMAD);
nbd = size(dirichlet_dof, 2);
BD = sparse(nbd, nbdofu);
for i = 1:nbd
    BD(i, dirichlet_dof(i)) = 1;
end


% Initial data
U0 = (gf_mesh_fem_get(mf_u, 'eval', {'exp(-100*((x-0.5).^2+(y-0.25).^2))'}))';
U00 = U0;
if (scheme == 1)
  V0 = -((B') \ (K' * U0));
end

% Time steps
NT = 200;
dt = 2*pi/NT;
if (scheme == 0)
  C2 = C * (-dt);
elseif (scheme == 1)
  C2 = C * (-dt)/2;
end
M = [K' B' BD'; B C2 sparse(nbdofv, nbd); BD sparse(nbd, nbdofv) sparse(nbd, nbd)];
ndraw = 10;
idraw = 10;


for t = 0:dt:2*pi

  if ((ndraw == idraw) || (t >= 2*pi-1e-8))
    figure(1);
    gf_plot(mf_u , U0', 'mesh', 'on', 'contour', .1:.1:2);
    caxis([0 1]);
    colorbar;
    pause(0.1);
    idraw = 0;
  end
  
  idraw = idraw + 1;

  if (scheme == 0)
    X0 = [zeros(nbdofu,1); (B*U0); zeros(nbd,1);];
  elseif (scheme == 1)
    X0 = [(-B'*V0-K'*U0); (B*U0+(C*V0)*dt/2); zeros(nbd,1);];
  end

  X1 = M\X0;

  U1 = X1(1:nbdofu);
  V1 = X1((nbdofu+1):(nbdofu+nbdofv));

  U0 = U1; V0 = V1;
  
end


figure(2);
gf_plot(mf_u , U00', 'mesh', 'on', 'contour', .1:.1:2);
caxis([0 1]);
colorbar;




