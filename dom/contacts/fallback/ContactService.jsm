/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

let DEBUG = 0;
if (DEBUG)
  debug = function (s) { dump("-*- Fallback ContactService component: " + s + "\n"); }
else
  debug = function (s) {}

const Cu = Components.utils; 
const Cc = Components.classes;
const Ci = Components.interfaces;

let EXPORTED_SYMBOLS = ["DOMContactManager"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/ContactDB.jsm");

XPCOMUtils.defineLazyGetter(this, "ppmm", function() {
  return Cc["@mozilla.org/parentprocessmessagemanager;1"].getService(Ci.nsIFrameMessageManager);
});

let myGlobal = this;

let DOMContactManager = {
  init: function() {
    debug("Init");
    this._messages = ["Contacts:Find", "Contacts:Clear", "Contact:Save", "Contact:Remove"];
    this._messages.forEach((function(msgName) {
      ppmm.addMessageListener(msgName, this);
    }).bind(this));

    var idbManager = Components.classes["@mozilla.org/dom/indexeddb/manager;1"].getService(Ci.nsIIndexedDatabaseManager);
    idbManager.initWindowless(myGlobal);
    this._db = new ContactDB(myGlobal);
    this._db.init(myGlobal);

    Services.obs.addObserver(this, "profile-before-change", false);

    try {
      let hosts = Services.prefs.getCharPref("dom.mozContacts.whitelist")
      hosts.split(",").forEach(function(aHost) {
        debug("Add host: " + JSON.stringify(aHost));
        if (aHost.length > 0)
          Services.perms.add(Services.io.newURI(aHost, null, null), "webcontacts-manage",
                             Ci.nsIPermissionManager.ALLOW_ACTION);
      });
    } catch(e) { debug(e); }
  },

  observe: function(aSubject, aTopic, aData) {
    myGlobal = null;
    this._messages.forEach((function(msgName) {
      ppmm.removeMessageListener(msgName, this);
    }).bind(this));
    Services.obs.removeObserver(this, "profile-before-change");
    ppmm = null;
    this._messages = null;
    if (this._db)
      this._db.close();
    this._db = null;
  },

  receiveMessage: function(aMessage) {
    function sortfunction(a, b){
      let x, y;
      if (a.properties[msg.findOptions.sortBy])
        x = a.properties[msg.findOptions.sortBy][0].toLowerCase();
      if (b.properties[msg.findOptions.sortBy])
        y = b.properties[msg.findOptions.sortBy][0].toLowerCase();
      let result = x.localeCompare(y);
      return msg.findOptions.sortOrder == 'ascending' ? result : -result;
    }
    debug("Fallback DOMContactManager::receiveMessage " + aMessage.name);
    let msg = aMessage.json;
    switch (aMessage.name) {
      case "Contacts:Find":
        let result = new Array();
        this._db.find(
          function(contacts) {
            for (let i in contacts)
              result.push(contacts[i]);
            if (msg.findOptions.sortOrder !== 'undefined' && msg.findOptions.sortBy !== 'undefined') {
              debug('sortBy: ' + msg.findOptions.sortBy + ', sortOrder: ' + msg.findOptions.sortOrder );
              result.sort(sortfunction);
              if (msg.findOptions.filterLimit)
                result = result.slice(0, msg.findOptions.filterLimit);
            }

            debug("result:" + JSON.stringify(result));
            ppmm.sendAsyncMessage("Contacts:Find:Return:OK", {requestID: msg.requestID, contacts: result});
          }.bind(this),
          function(aErrorMsg) { ppmm.sendAsyncMessage("Contacts:Find:Return:KO", { requestID: msg.requestID, errorMsg: aErrorMsg }) }.bind(this),
          msg.findOptions);
        break;
      case "Contact:Save":
        this._db.saveContact(
          msg.contact, 
          function() { ppmm.sendAsyncMessage("Contact:Save:Return:OK", { requestID: msg.requestID, contactID: msg.contact.id }); }.bind(this),
          function(aErrorMsg) { ppmm.sendAsyncMessage("Contact:Save:Return:KO", { requestID: msg.requestID, errorMsg: aErrorMsg }); }.bind(this)
        );
        break;
      case "Contact:Remove":
        this._db.removeContact(
          msg.id, 
          function() { ppmm.sendAsyncMessage("Contact:Remove:Return:OK", { requestID: msg.requestID, contactID: msg.id }); }.bind(this),
          function(aErrorMsg) { ppmm.sendAsyncMessage("Contact:Remove:Return:KO", { requestID: msg.requestID, errorMsg: aErrorMsg }); }.bind(this)
        );
        break;
      case "Contacts:Clear":
        this._db.clear(
          function() { ppmm.sendAsyncMessage("Contacts:Clear:Return:OK", { requestID: msg.requestID }); }.bind(this),
          function(aErrorMsg) { ppmm.sendAsyncMessage("Contacts:Clear:Return:KO", { requestID: msg.requestID, errorMsg: aErrorMsg }); }.bind(this)
        );
    }
  }
}

DOMContactManager.init();
