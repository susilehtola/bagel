//
// BAGEL - Brilliantly Advanced General Electronic Structure Library
// Filename: MSCASPT2.cc
// Copyright (C) 2014 Toru Shiozaki
//
// Author: Toru Shiozaki <shiozaki@northwestern.edu>
// Maintainer: Shiozaki group
//
// This file is part of the BAGEL package.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include <bagel_config.h>
#ifdef COMPILE_SMITH


#include <src/smith/caspt2/MSCASPT2.h>

using namespace std;
using namespace bagel;
using namespace bagel::SMITH;


MSCASPT2::MSCASPT2::MSCASPT2(const CASPT2::CASPT2& cas) {
  info_    = cas.info_;
  virt_    = cas.virt_;
  active_  = cas.active_;
  closed_  = cas.closed_;
  rvirt_   = cas.rvirt_;
  ractive_ = cas.ractive_;
  rclosed_ = cas.rclosed_;
  heff_    = cas.heff_;

  t2all_ = cas.t2all_;
  lall_  = cas.lall_;
  den1 = cas.h1_->clone();
  den2 = cas.h1_->clone();
  Den1 = cas.init_residual();

  rdm0all_ = cas.rdm0all_;
  rdm1all_ = cas.rdm1all_;
  rdm2all_ = cas.rdm2all_;
  rdm3all_ = cas.rdm3all_;
  rdm4all_ = cas.rdm4all_;
}


void MSCASPT2::MSCASPT2::solve_deriv() {
  // first-order energy from the energy expression
  // TODO these can be computed more efficiently using mixed states
  const int nstates = info_->ciwfn()->nstates();
  const int target = info_->target();
  {
    shared_ptr<Tensor> result = den1->clone();
    shared_ptr<Tensor> result2 = Den1->clone();
    for (int jst = 0; jst != nstates; ++jst) { // bra
      const double jheff = (*heff_)(jst, target);
      for (int ist = 0; ist != nstates; ++ist) { // ket
        set_rdm(jst, ist);
        for (int istate = 0; istate != nstates; ++istate) { // state of T
          l2 = t2all_[istate]->at(ist); // careful
          shared_ptr<Queue> queue = make_density1q(true, ist == jst);
          while (!queue->done())
            queue->next_compute();
          result->ax_plus_y((*heff_)(istate, target)*jheff, den1);

          shared_ptr<Queue> queue2 = make_density2q(true, ist == jst);
          while (!queue2->done())
            queue2->next_compute();
          result2->ax_plus_y((*heff_)(istate, target)*jheff, Den1);
        }
      }
    }
    den1_ = result->matrix();
    Den1_ = result2;
  }
  // den1_->print(); // looks correct to me
  // second order contribution
  {
    shared_ptr<Tensor> result = den2->clone();
    for (int jst = 0; jst != nstates; ++jst) { // bra
      for (int ist = 0; ist != nstates; ++ist) { // ket
        set_rdm(jst, ist);
        for (int istate = 0; istate != nstates; ++istate) { // state of T
          l2 = lall_[istate]->at(ist);
          t2 = t2all_[istate]->at(jst);
          shared_ptr<Queue> queue = make_densityq(true, ist == jst);
          while (!queue->done())
            queue->next_compute();
          result->ax_plus_y(1.0, den2);
        }
      }
    }
    den2_ = result->matrix();
  }
  {
    shared_ptr<Tensor> result = den1->clone();
    shared_ptr<Tensor> result2 = Den1->clone();
    for (int jst = 0; jst != nstates; ++jst) { // bra
      for (int ist = 0; ist != nstates; ++ist) { // ket
        set_rdm(jst, ist);

        l2 = lall_[jst]->at(ist);
        shared_ptr<Queue> queue = make_density1q(true, ist == jst);
        while (!queue->done())
          queue->next_compute();
        result->ax_plus_y(1.0, den1);

        shared_ptr<Queue> queue2 = make_density2q(true, ist == jst);
        while (!queue2->done())
          queue2->next_compute();
        result2->ax_plus_y(1.0, Den1);
      }
    }
    den1_->ax_plus_y(1.0, result->matrix());
    Den1_->ax_plus_y(1.0, result2);
  }
}

#endif