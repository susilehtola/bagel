//
// BAGEL - Parallel electron correlation program.
// Filename: momentum_london.cc
// Copyright (C) 2014 Toru Shiozaki
//
// Author: Ryan D. Reynolds <RyanDReynolds@u.northwestern.edu>
// Maintainer: Shiozaki group
//
// This file is part of the BAGEL package.
//
// The BAGEL package is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 3, or (at your option)
// any later version.
//
// The BAGEL package is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with the BAGEL package; see COPYING.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//


#include <src/prop/momentum_london.h>
#include <src/integral/compos/complexmomentumbatch.h>

using namespace std;
using namespace bagel;

Momentum_London::Momentum_London(shared_ptr<const Geometry> g) : geom_(g) {
  assert(geom_->magnetism());
}


array<shared_ptr<ZMatrix>, 3> Momentum_London::compute() const {

  const int nbasis = geom_->nbasis();
  auto mat0 = make_shared<ZMatrix>(nbasis, nbasis);
  auto mat1 = make_shared<ZMatrix>(nbasis, nbasis);
  auto mat2 = make_shared<ZMatrix>(nbasis, nbasis);

  // TODO perhaps we could reduce operation by a factor of 2
  auto o0 = geom_->offsets().begin();
  for (auto a0 = geom_->atoms().begin(); a0 != geom_->atoms().end(); ++a0, ++o0) {
    auto o1 = geom_->offsets().begin();
    for (auto a1 = geom_->atoms().begin(); a1 != geom_->atoms().end(); ++a1, ++o1) {

      auto offset0 = o0->begin();
      for (auto b0 = (*a0)->shells().begin(); b0 != (*a0)->shells().end(); ++b0, ++offset0) {
        auto offset1 = o1->begin();
        for (auto b1 = (*a1)->shells().begin(); b1 != (*a1)->shells().end(); ++b1, ++offset1) {

          array<shared_ptr<const Shell>,2> input = {{*b1, *b0}};
          ComplexMomentumBatch mom(input, geom_->magnetic_field());
          mom.compute();

          const complex<double>* dat0 = mom.data();
          const complex<double>* dat1 = mom.data() + mom.size_block();
          const complex<double>* dat2 = mom.data() + mom.size_block()*2;
          for (int i = *offset0; i != *offset0 + (*b0)->nbasis(); ++i) {
            for (int j = *offset1; j != *offset1 + (*b1)->nbasis(); ++j, ++dat0, ++dat1, ++dat2) {
              mat0->element(j,i) = *dat0;
              mat1->element(j,i) = *dat1;
              mat2->element(j,i) = *dat2;
            }
          }

        }
      }
    }
  }

  return array<shared_ptr<ZMatrix>,3>{{mat0, mat1, mat2}};
}