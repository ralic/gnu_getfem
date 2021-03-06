.. $Id: intro.rst 4471 2013-11-21 19:44:22Z renard $

.. include:: ../replaces.txt

.. highlightlang:: c++

.. _dp-contribute:

How to contribute / Git repository on Savannah
==============================================

.. |saweb| replace:: Savannah
.. _saweb: https://savannah.gnu.org

.. |sawebg| replace:: Getfem on Savannah
.. _sawebg: https://savannah.nongnu.org/projects/getfem

.. |linktask| replace:: https://savannah.nongnu.org/task/?group=getfem
.. _linktask: https://savannah.nongnu.org/task/?group=getfem


|gf| is an  open source finite element library based on a collaborative development. If you intend to make some contributions, you can ask for membership of the project there. Contributions of all kinds are welcome: documentation, bug reports, constructive comments, changes suggestions, bug fix, new models, etc ...

Contributors are of course required to be careful that their changes do not affect the proper functioning of the library and that these changes follow a principle of backward compatibility.

See |linktask|_ for a list of task and discussions about |gf| development.

**IMPORTANT** : a contributor implicitly accepts that his/her contribution will be distributed under the LGPL licence of |gf|.

The main repository of |gf| is on Savannah, the software forge of the Free Software Foundation (see |sawebg|_). The page of the project on Savannah is |sawebg|_


How to get the sources
----------------------

.. |sagit| replace:: git on Savannah
.. _sagit: http://savannah.gnu.org/maintenance/UsingGit/

If you just want the sources and do not intend to make some contributions, you can just use the command ::

  git clone https://git.savannah.nongnu.org/git/getfem.git

If you intend to make some contributions, the first step is to ask for the inclusion in the |gf| project (for this you have to create a Savannah account). You have also to register a ssh key (see |sagit|_) and then use the command ::

  git clone ssh://savannah-login@git.sv.gnu.org:/srv/git/getfem.git

How to contribute
-----------------

Before modifying any file, you have to create a *development branch* because it is *not allowed to make a modification directly in the master branch*. It is recommended that the branch name is of the type `devel-name` where name is your name or login. For instance, if you chose `devel-me` as the branch name, the creation of the branch reads ::

  git branch devel-me
  git checkout devel-me

The first command create the branch and the second one position you on your branch. After that you are nearly ready to makes some modifications. You can specify your contact name and e-mail with the following commands in order to label your changes ::

  git config --global user.name "Your Name Comes Here"
  git config --global user.email you@yourdomain.example.com

Locally commit your changes
---------------------------

Once you made some modifications of a file or you added a new file, say `src/toto.cc`, the local commit is done with the commands::

  git add src/toto.cc
  git commit -m "Your extensive commit message here"

At this stage the commit is done on your local repository but not in the Savannah one.

Push you changes in the Savannah repository
-------------------------------------------

You can now transfer your modifications to the Savannah repository with ::

  git push origin devel-me

where of course *devel-me* is still the name of your branch. At this stage your modifications are registered in the branch *devel-me* of Savannah repository. 
Your role stops here, since you are not allowed to modify the master branch of |gf|.


Ask for an admin to merge your modifications to the master branch of |gf|
-------------------------------------------------------------------------

Once you validated your modifications with sufficient tests, you can ask an admin of |gf| to merge your modifications. For this you just have to send an e-mail to *getfem-commits@Savannah.org* with the message : "please merge branch devel-me" with eventually a short description of the modifications. 


Merge modifications done by other contributors
----------------------------------------------

Regularly, you can run a ::

  git pull origin master
  git merge master

in order to integrate the modifications which has been validated and integrated to the master branch. This is recommended to run this command before any request for integration of a modification in the master branch.


Some useful git commands
------------------------
::

  git status  : status of your repository / branch

  git log --follow "filepath"   : Show all the commits modifying the specified file (and follow the eventual change of name of the file).

  gitk --follow filename : same as previous but with a graphical interface
