/**
 * Copyright (c) 2014-present, The osquery authors
 *
 * This source code is licensed as defined by the LICENSE file found in the
 * root directory of this source tree.
 *
 * SPDX-License-Identifier: (Apache-2.0 OR GPL-2.0-only)
 */

#include <string>

#include <atlbase.h>
#include <netfw.h>
#include <windows.h>

#include <osquery/core/core.h>
#include <osquery/core/tables.h>
#include <osquery/logger/logger.h>
#include <osquery/utils/conversions/windows/strings.h>

namespace osquery {
namespace tables {

HRESULT populateRow(INetFwRule* rule, Row& r);

QueryData genDefenderFirewallRules(QueryContext& context) {
  QueryData results;

  CComPtr<INetFwPolicy2> netFwPolicy;
  HRESULT hr = netFwPolicy.CoCreateInstance(CLSID_NetFwPolicy2);
  if (FAILED(hr)) {
    TLOG << "Failed to instantiate INetFwPolicy2";
    return results;
  }

  CComPtr<INetFwRules> rules;
  hr = netFwPolicy->get_Rules(&rules);
  if (FAILED(hr)) {
    TLOG << "Failed to get firewall rules";
    return results;
  }

  CComPtr<IUnknown> enumerator;
  hr = rules->get__NewEnum(&enumerator);
  if (FAILED(hr)) {
    TLOG << "Failed to get firewall rules enumerator";
    return results;
  }

  CComPtr<IEnumVARIANT> enumvar;
  hr = enumerator->QueryInterface(IID_PPV_ARGS(&enumvar));
  if (FAILED(hr)) {
    TLOG << "Failed to get firewall rules enumerator interface";
    return results;
  }

  do {
    CComVariant value;
    ULONG fetched = 0;
    hr = enumvar->Next(1, &value, &fetched);
    if (FAILED(hr)) {
      TLOG << "Failed to enumerate next firewall rule";
      break;
    }

    if (hr != S_FALSE) {
      hr = value.ChangeType(VT_DISPATCH);
      if (FAILED(hr)) {
        TLOG << "Failed to get IDispatch for enumerated rule";
        break;
      }

      CComPtr<INetFwRule> rule;
      hr = (V_DISPATCH(&value))
               ->QueryInterface(__uuidof(INetFwRule), (void**)&rule);
      if (FAILED(hr)) {
        TLOG << "Failed to get firewall rule interface";
        break;
      }

      Row r;
      hr = populateRow(rule, r);
      if (FAILED(hr)) {
        TLOG << "Failed to convert firewall rule to row";
        break;
      }

      results.push_back(std::move(r));
    }
  } while (SUCCEEDED(hr) && hr != S_FALSE);

  return results;
}

HRESULT populateRow(INetFwRule* rule, Row& r) {
  HRESULT hr = S_OK;

  CComBSTR name;
  if (FAILED(hr = rule->get_Name(&name))) {
    return hr;
  }
  r["name"] = bstrToString(name);

  CComBSTR appname;
  if (FAILED(hr = rule->get_ApplicationName(&appname))) {
    return hr;
  }
  r["app_name"] = bstrToString(appname);

  NET_FW_ACTION action;
  if (FAILED(hr = rule->get_Action(&action))) {
    return hr;
  }
  switch (action) {
  case NET_FW_ACTION_BLOCK:
    r["action"] = "Block";
    break;
  case NET_FW_ACTION_ALLOW:
    r["action"] = "Allow";
    break;
  }

  VARIANT_BOOL enabled;
  if (FAILED(hr = rule->get_Enabled(&enabled))) {
    return hr;
  }
  r["enabled"] = INTEGER(bool(enabled));

  NET_FW_RULE_DIRECTION direction;
  if (FAILED(hr = rule->get_Direction(&direction))) {
    return hr;
  }
  switch (direction) {
  case NET_FW_RULE_DIR_IN:
    r["direction"] = "In";
    break;
  case NET_FW_RULE_DIR_OUT:
    r["direction"] = "Out";
    break;
  }

  long protocol = 0;
  if (FAILED(hr = rule->get_Protocol(&protocol))) {
    return hr;
  }
  switch (protocol) {
  case NET_FW_IP_PROTOCOL_TCP:
    r["protocol"] = "TCP";
    break;
  case NET_FW_IP_PROTOCOL_UDP:
    r["protocol"] = "UDP";
    break;
  case NET_FW_IP_PROTOCOL_ANY:
    r["protocol"] = "Any";
    break;
  }

  long profileBitmask = 0;
  if (FAILED(hr = rule->get_Profiles(&profileBitmask))) {
    return hr;
  }

  std::string profiles;
  if (profileBitmask == NET_FW_PROFILE2_ALL) {
    r["profiles"] = "Any";
  } else {
    std::string profile;
    if (profileBitmask & NET_FW_PROFILE2_DOMAIN) {
      profiles = "Domain";
    }
    if (profileBitmask & NET_FW_PROFILE2_PRIVATE) {
      if (!profiles.empty()) {
        profiles += ",";
      }
      profiles += "Private";
    }
    if (profileBitmask & NET_FW_PROFILE2_PUBLIC) {
      if (!profiles.empty()) {
        profiles += ",";
      }
      profiles += "Public";
    }
    if (!profiles.empty()) {
      r["profiles"] = profiles;
    }
  }
  return hr;
}

} // namespace tables
} // namespace osquery
