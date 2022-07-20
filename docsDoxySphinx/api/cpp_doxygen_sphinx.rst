**************
C++17 API
**************

.. caution::

  DO NOT use ``using namespace redev`` in your code.
  This is in general a bad practices that creates potential name
  conflicts.
  Always use ``redev::`` explicitly, *e.g.* ``redev::Redev``.

Types
-----

.. doxygentypedef:: redev::LO
   :project: Redev

.. doxygentypedef:: redev::LOs
   :project: Redev

.. doxygentypedef:: redev::GO
   :project: Redev

.. doxygentypedef:: redev::GOs
   :project: Redev

.. doxygentypedef:: redev::Real
   :project: Redev

.. doxygentypedef:: redev::Reals
   :project: Redev

.. doxygentypedef:: redev::CV
   :project: Redev

.. doxygentypedef:: redev::CVs
   :project: Redev


`PartitionInterface`
-----------

.. doxygenclass:: redev::PartitionInterface
   :project: Redev

`ClassPtn`
----------

.. doxygenclass:: redev::ClassPtn
   :project: Redev

`RCBPtn`
----------

.. doxygenclass:: redev::RCBPtn
   :project: Redev


`Redev`
-------

.. doxygenclass:: redev::Redev
   :project: Redev

`BidirectionalComm`
-------------------

.. doxygenclass:: redev::BidirectionalComm
   :project: Redev

`Communicator`
--------------

.. doxygenclass:: redev::Communicator
   :project: Redev

`AdiosComm`
-----------

.. doxygenclass:: redev::AdiosComm
   :project: Redev

`InMessageLayout`
-----------------

.. doxygenstruct:: redev::InMessageLayout
   :project: Redev

