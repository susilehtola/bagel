//
// BAGEL - Parallel electron correlation program.
// Filename: fci_rdmderiv.cc
// Copyright (C) 2011 Toru Shiozaki
//
// Author: Toru Shiozaki <shiozaki@northwestern.edu>
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


#include <src/ci/fci/fci.h>
#include <src/util/math/algo.h>
#include <src/wfn/rdm.h>

using namespace std;
using namespace bagel;


shared_ptr<Dvec> FCI::rdm1deriv(const int target) const {

  auto detex = make_shared<Determinants>(norb_, nelea_, neleb_, false, /*mute=*/true);
  cc_->set_det(detex);
  shared_ptr<Civec> cbra = cc_->data(target);

  // 1RDM ci derivative
  // <I|E_ij|0>

  auto dbra = make_shared<Dvec>(cbra->det(), norb_*norb_);
  dbra->zero();
  sigma_2a1(cbra, dbra);
  sigma_2a2(cbra, dbra);

  return dbra;
}


shared_ptr<Dvec> FCI::rdm2deriv(const int target) const {

  auto detex = make_shared<Determinants>(norb_, nelea_, neleb_, false, /*mute=*/true);
  cc_->set_det(detex);
  shared_ptr<Civec> cbra = cc_->data(target);

  // make  <I|E_ij|0>
  auto dbra = make_shared<Dvec>(cbra->det(), norb_*norb_);
  dbra->zero();
  sigma_2a1(cbra, dbra);
  sigma_2a2(cbra, dbra);

  // second make <J|E_kl|I><I|E_ij|0> - delta_li <J|E_kj|0>
  auto ebra = make_shared<Dvec>(cbra->det(), norb_*norb_*norb_*norb_);
  auto tmp = make_shared<Dvec>(cbra->det(), norb_*norb_);
  int ijkl = 0;
  int ij = 0;
  for (auto iter = dbra->dvec().begin(); iter != dbra->dvec().end(); ++iter, ++ij) {
    const int j = ij/norb_;
    const int i = ij-j*norb_;
    tmp->zero();
    sigma_2a1(*iter, tmp);
    sigma_2a2(*iter, tmp);
    int kl = 0;
    for (auto t = tmp->dvec().begin(); t != tmp->dvec().end(); ++t, ++ijkl, ++kl) {
      *ebra->data(ijkl) = **t;
      const int l = kl/norb_;
      const int k = kl-l*norb_;
      if (l == i) *ebra->data(ijkl) -= *dbra->data(k+norb_*j);
    }
  }
  return ebra;
}


tuple<shared_ptr<Dvec>,shared_ptr<Dvec>> FCI::rdm34deriv(const int target, shared_ptr<const Matrix> fock) const {
  assert(fock->ndim() == norb_ && fock->mdim() == norb_);
  auto detex = make_shared<Determinants>(norb_, nelea_, neleb_, false, /*mute=*/true);
  cc_->set_det(detex);
  shared_ptr<Civec> cbra = cc_->data(target);

  const size_t norb2 = norb_*norb_;
  const size_t norb3 = norb2*norb_;
  const size_t norb4 = norb2*norb2;
  const size_t norb5 = norb3*norb2;
  const size_t norb6 = norb4*norb2;

  // first make <I|i+j|0>
  auto dbra = rdm1deriv(target);
  // second make <J|k+i+jl|0> = <J|k+l|I><I|i+j|0> - delta_li <J|k+j|0>
  auto ebra = rdm2deriv(target);

  // third make <K|m+k+i+jln|0>  =  <K|m+n|J><J|k+i+jl|0> - delta_nk<K|m+i+jl|0> - delta_ni<K|k+m+jl|0>
  auto fbra = make_shared<Dvec>(cbra->det(), norb6);
  {
    auto tmp = make_shared<Dvec>(cbra->det(), norb2);
    int ijkl = 0;
    for (auto iter = ebra->dvec().begin(); iter != ebra->dvec().end(); ++iter, ++ijkl) {
      const int j = ijkl/norb3;
      const int i = ijkl/norb2-j*norb_;
      const int l = ijkl/norb_-i*norb_-j*norb2;
      const int k = ijkl-l*norb_-i*norb2-j*norb3;
      tmp->zero();
      sigma_2a1(*iter, tmp);
      sigma_2a2(*iter, tmp);
      for (int m = 0; m != norb_; ++m) {
        for (int n = 0; n != norb_; ++n)
          *fbra->data(ijkl+norb4*m+norb5*n) += *tmp->data(m+norb_*n);
        *fbra->data(ijkl+norb4*m+norb5*k) -= *ebra->data(m+norb_*(l+norb_*(i+norb_*(j))));
        *fbra->data(ijkl+norb4*m+norb5*i) -= *ebra->data(k+norb_*(l+norb_*(m+norb_*(j))));
      }
    }
  }

  // now make target:  f_mn <L|E_op,mn,kl,ij|0> = [L|E_kl,ij,op|0]  =  <L|o+p|K>[K|E_kl,ij|0] - f_np<L|E_kl,ij,on|0> - delta_pk[L|E_ij,ol|0] - delta_pi[L|E_kl,oj|0]
  // calculate [K|k+i+jl|0]
  auto fock_fbra = ebra->clone();
  dgemv_("N", fbra->size()/norb2, norb2, 1.0, fbra->data(), fbra->size()/norb2, fock->data(), 1, 0.0, fock_fbra->data(), 1);

  auto gbra = fbra->clone();
  {
    auto tmp = make_shared<Dvec>(cbra->det(), norb2);
    int ijkl = 0;
    for (auto iter = fock_fbra->dvec().begin(); iter != fock_fbra->dvec().end(); ++iter, ++ijkl) {
      const int j = ijkl/norb3;
      const int i = ijkl/norb2-j*norb_;
      const int l = ijkl/norb_-i*norb_-j*norb2;
      const int k = ijkl-l*norb_-i*norb2-j*norb3;
      tmp->zero();
      sigma_2a1(*iter, tmp);
      sigma_2a2(*iter, tmp);
      for (int o = 0; o != norb_; ++o) {
        for (int p = 0; p != norb_; ++p)
          *gbra->data(ijkl+norb4*o+norb5*p) += *tmp->data(o+norb_*p);
        *gbra->data(ijkl+norb4*o+norb5*k) -= *fock_fbra->data(i+norb_*(j+norb_*(o+norb_*l)));
        *gbra->data(ijkl+norb4*o+norb5*i) -= *fock_fbra->data(k+norb_*(l+norb_*(o+norb_*j)));
      }
    }
    dgemm_("N", "N", fbra->size()/norb_, norb_, norb_, -1.0, fbra->data(), fbra->size()/norb_, fock->data(), norb_, 1.0, gbra->data(), fbra->size()/norb_);
  }

  return make_tuple(fbra, gbra);
}