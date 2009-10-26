function pde = init_pde()
pde = mlist(['pde', ...
             'verbosity', ... // integer
             'mim', ...
             'type', ... 
             'lambda', ...
             'mu', ...
             'viscos', ...
             'K', ...
             'H', ...
             'R', ...
             'Q', ...
             'F', ...
             'G', ...
             'B', ...
             'RK', ...
             'asm', ... 
             'solver', ... 
             'mf_u', ...
             'mf_d', ...
             'mf_p', ...
             'PR', ...
             'E', ...
             'pdetool', ... // pde('pdetool')('b'), pde('pdetool')('e'), 
             'bound', ...
             ]); 

pde('verbosity')     = 0; // integer
pde('type')          = [];     // 'laplacian', 'linear elasticity', 'stockes'
pde('lambda')        = [];
pde('mu')            = [];
pde('viscos')        = [];
pde('K')             = [];
pde('H')             = [];
pde('R')             = [];
pde('Q')             = [];
pde('F')             = [];
pde('G')             = [];
pde('B')             = [];
pde('RK')            = [];
pde('asm')           = mlist(['asm', 'lambda', 'mu', 'viscos', 'K', 'H', 'R', 'Q', 'F', 'G', 'B', 'RK']);
pde('asm')('lambda') = [];
pde('asm')('mu')     = [];
pde('asm')('viscos') = [];
pde('asm')('K')      = [];
pde('asm')('H')      = [];
pde('asm')('R')      = [];
pde('asm')('Q')      = [];
pde('asm')('F')      = [];
pde('asm')('G')      = [];
pde('asm')('B')      = [];
pde('asm')('RK')     = [];
pde('solver')        = 'default' // 'brute_stockes', 'default', set_default_values(pde('solver'),'type','cg','maxiter',1000,'residu',1e-6);
pde('mim')           = [];
pde('mf_u')          = [];
pde('mf_d')          = [];
pde('mf_p')          = [];
pde('PR')            = [];
pde('E')             = [];
pde('pdetool')       = mlist(['pdetool','b','e']);
pde('pdetool')('b')  = [];
pde('pdetool')('e')  = [];
pde('bound')         = []; // 'list' + 'type' + 'R' + 'H'
endfunction
