/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var ScratchpadManager = Scratchpad.ScratchpadManager;

/* Call the iterator for each item in the list,
   calling the final callback with all the results
   after every iterator call has sent its result */
function asyncMap(items, iterator, callback)
{
  let expected = items.length;
  let results = [];

  items.forEach(function(item) {
    iterator(item, function(result) {
      results.push(result);
      if (results.length == expected) {
        callback(results);
      }
    });
  });
}

function test()
{
  waitForExplicitFinish();
  testRestore();
}

function testRestore()
{
  let states = [
    {
      filename: "testfile",
      text: "test1",
      executionContext: 2
    },
    {
      text: "text2",
      executionContext: 1
    },
    {
      text: "text3",
      executionContext: 1
    }
  ];

  asyncMap(states, function(state, done) {
    // Open some scratchpad windows
    let win = ScratchpadManager.openScratchpad(state);
    win.addEventListener("load", function onScratchpadLoad() {
      removeEventListener("load", onScratchpadLoad, false);
      done(win);
    }, false)
  }, function(wins) {
    // Then save the windows to session store
    ScratchpadManager.saveOpenWindows();

    // Then get their states
    let session = ScratchpadManager.getSessionState();

    // Then close them
    wins.forEach(function(win) {
      win.close();
    });

    // Clear out session state for next tests
    ScratchpadManager.saveOpenWindows();

    // Then restore them
    let restoredWins = ScratchpadManager.restoreSession(session);

    is(restoredWins.length, 3, "Three scratchad windows restored");

    asyncMap(restoredWins, function(restoredWin, done) {
      restoredWin.addEventListener("load", function onScratchpadLoad() {
        restoredWin.removeEventListener("load", onScratchpadLoad, false);
        let state = restoredWin.Scratchpad.getState();
        restoredWin.close();
        done(state);
      }, false);
    }, function(restoredStates) {
      // Then make sure they were restored with the right states
      ok(statesMatch(restoredStates, states),
        "All scratchpad window states restored correctly");

      // Yay, we're done!
      finish();
    });
  });
}

function statesMatch(restoredStates, states)
{
  return states.every(function(state) {
    return restoredStates.some(function(restoredState) {
      return state.filename == restoredState.filename
        && state.text == restoredState.text
        && state.executionContext == restoredState.executionContext;
    })
  });
}
