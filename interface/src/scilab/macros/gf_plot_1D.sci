function [hline, hdof] = gf_plot_1D(mf,U, varargin)
// function h=gf_plot_1D(mf,U,...)
// this function plots a 1D finite element field.
//
// The options are specified as pairs of 'option name'/'option value'
//  'style', 'bo-'       : line style and dof marker style (same
//                         syntax as in the Scilab command 'plot');
//  'color', ''          : override the line color;
//  'dof_color', 'red'   : color of markers for degrees of freedom;
//  'width', 2           : line width.

opts = build_options_list(varargin(:));

try 
  gf_workspace('push', 'gf_plot_1D');
  [hline, hdof] = gf_plot_1D_aux(mf,U, opts);
catch
  [str,n,line,func]=lasterror();
  disp('error in gf_plot_1D: ' + str);
  disp(sprintf('error %d in %s at line %d\n', n, func, line));
  error('');
end
gf_workspace('pop');
endfunction

  
function [hline, hdof] = gf_plot_1D_aux(mf, U, opts)

[opt_style,err]      = get_param(opts,'style','bo-');
[opt_color,err]      = get_param(opts,'color','');
[opt_dof_color,err]  = get_param(opts,'dof_color','red');
[opt_width,err]      = get_param(opts,'width',2);

// try to remove markers from the line style
s              = opt_style; 
opt_style      = ''; 
opt_dof_marker = '';

for i=s
  if (isempty(strindex('o.x+*sdv^<>p',i))) then 
    opt_style = opt_style + i;
  else
    opt_dof_marker = i;
  end
end

// save graphical context
cax = gcf();

nbd = gf_mesh_fem_get(mf, 'nbdof');
if (nbd < 100) then
  REFINE = 32;
elseif (nbd < 1000) then
  REFINE = 6;
else
  REFINE = 2;
end

m = gf_mesh_fem_get(mf, 'linked_mesh');
sl = gf_slice(list('none'),m, REFINE); 
Usl = gf_compute(mf,U,'interpolate on', sl);
D = unique(gf_mesh_fem_get(mf, 'basic dof nodes'));
slD = gf_slice('points', m, D);
UD = gf_compute(mf,U,'interpolate on',slD);

X = gf_slice_get(sl, 'pts');
Y = Usl;
plot(X, Y, opt_style); 
hline = gce();
hline.children.thickness = opt_width;
if (~isempty(opt_color)) then 
  hline.children.foreground = color(opt_color);
end

hdof = [];
if (~isempty(opt_dof_marker)) then
  plot(gf_slice_get(slD, 'pts'), UD, opt_dof_marker);
  hdof = gce();
  hdof.children.mark_foreground = color(opt_dof_color);
end
endfunction

