String INDEX_HTML = R"(<!DOCTYPE html>
<html>
<head>
  <meta http-equiv='Cache-Control' content='no-cache, no-store, must-revalidate' />
  <meta http-equiv='Pragma' content='no-cache' />
  <meta http-equiv='Expires' content='0' />
  <title>{NAME}</title>
  <link rel='stylesheet' href='https://unpkg.com/onsenui/css/onsenui.css'>
  <link rel='stylesheet' href='https://unpkg.com/onsenui/css/onsen-css-components.min.css'>
  <script src='https://unpkg.com/onsenui/js/onsenui.min.js'></script>
  <script src='https://unpkg.com/jquery/dist/jquery.min.js'></script>
  <script>
  var cversion = '{VERSION}';
  var wsUri = 'ws://'+location.host+':81/';
  var repo = 'esp32-motorized-roller-blinds';
  var connectedSteppers = [{CONNECTED_STEPPERS}];

  window.fn = {};
  window.fn.open = function() {
    var menu = document.getElementById('menu');
    menu.open();
  };

  window.fn.load = function(page) {
    console.log("Loading page:", page);
    var content = document.getElementById('content');
    var menu = document.getElementById('menu');
    content.load(page)
      .then(menu.close.bind(menu))
      .then(updateControls(page));
  };

  var renderSettingsControls = function() {
    const rootNode = $('#page-settings')[0];

    connectedSteppers.forEach(stepperNum => {
        rootNode.appendChild(
            ons.createElement(`
              <ons-card>
                <div class='title'>Blind ${stepperNum}</div>
                <ons-row style='width:100%'>
                  <ons-col style='text-align:center'><ons-icon id='arrow-up-man${stepperNum}' icon='fa-arrow-up' size='2x'></ons-icon></ons-col>
                  <ons-col style='text-align:center'><ons-icon id='arrow-stop-man${stepperNum}' icon='fa-stop' size='2x'></ons-icon></ons-col>
                  <ons-col style='text-align:center'><ons-icon id='arrow-down-man${stepperNum}' icon='fa-arrow-down' size='2x'></ons-icon></ons-col>
                <ons-row style='width:100%'>
                  <ons-col style='text-align:center'><ons-button id='set-start${stepperNum}'>Set Start</ons-button></ons-col>
                  <ons-col style='text-align:center'>&nbsp;</ons-col>
                  <ons-col style='text-align:center'><ons-button id='set-max${stepperNum}'>Set Max</ons-button></ons-col>
                </ons-row>
              </ons-card>
            `)
        );
    });
  };

  var renderSteppersControls = function() {
    const steppersList = $('#steppers-position')[0];
    connectedSteppers.forEach(stepperNum => {
        steppersList.appendChild(ons.createElement(`
            <ons-row>
                <ons-col width='40px' style='text-align: center; line-height: 31px;'>
                </ons-col>
                <ons-col>
                    <ons-progress-bar id='pbar${stepperNum}' value='75'></ons-progress-bar>
                </ons-col>
                <ons-col width='40px' style='text-align: center; line-height: 31px;'>
                </ons-col>
            </ons-row>`));
        steppersList.appendChild(ons.createElement(`
            <ons-row>
                <ons-col width='40px' style='text-align: center; line-height: 31px;'>
                <ons-icon id='arrow-open${stepperNum}' icon='fa-arrow-up' size='2x'></ons-icon>
                </ons-col>
                <ons-col>
                <ons-range id='setrange${stepperNum}' style='width: 100%;' value='0'></ons-range>
                </ons-col>
                <ons-col width='40px' style='text-align: center; line-height: 31px;'>
                <ons-icon id='arrow-close${stepperNum}' icon='fa-arrow-down' size='2x'></ons-icon>
                </ons-col>
            </ons-row>
        `));
    });
  };

  var updateControls = function(page) {
    console.log('Updating controls on page:', page);
    doSend('{"action": "update"}');
    $.get('https://api.github.com/repos/eg321/'+repo+'/releases', function(data){
      if (data.length>0 && data[0].tag_name !== cversion){
        $('#cversion').text(cversion);
        $('#nversion').text(data[0].tag_name);
        $('#update-card').show();
      }
    });

    setTimeout(function() {
      console.log('Connected steppers:', connectedSteppers);
      if (!page || page == 'home.html') {
        renderSteppersControls();
      } else if (page == 'settings.html') {
        renderSettingsControls();
      }

      connectedSteppers.forEach(function(stepperNum) {
          $('#arrow-close' + stepperNum).on('click', function(){$('#setrange' + stepperNum).val(100);doSend('{"id": ' + stepperNum + ', "action": "auto", "value": 100}');});
          $('#arrow-open' + stepperNum).on('click', function(){$('#setrange' + stepperNum).val(0);doSend('{"id": ' + stepperNum + ', "action": "auto", "value": 0}');});
          $('#setrange' + stepperNum).on('change', function(){doSend('{"id": ' + stepperNum + ', "action": "auto", "value": ' + $('#setrange' + stepperNum).val() + '}')});

          $('#arrow-up-man' + stepperNum).on('click', function(){doSend('{"id": ' + stepperNum + ', "action": "manual", "value": -1}')});
          $('#arrow-down-man' + stepperNum).on('click', function(){doSend('{"id": ' + stepperNum + ', "action": "manual", "value": 1}')});
          $('#arrow-stop-man' + stepperNum).on('click', function(){doSend('{"id": ' + stepperNum + ', "action": "manual", "value": 0}')});
          $('#set-start' + stepperNum).on('click', function(){doSend('{"id": ' + stepperNum + ', "action": "start", "value":0}')});
          $('#set-max' + stepperNum).on('click', function(){doSend('{"id": ' + stepperNum + ', "action": "max", "value": 0}')});
      });

    }, 200);
  };

  $(document).ready(function() {
    updateControls();
  });

  var websocket;
  var timeOut;
  function retry(){
    clearTimeout(timeOut);
    timeOut = setTimeout(function(){
      websocket=null; init();},5000);
  };
  function init(){
    ons.notification.toast({message: 'Connecting...', timeout: 1000});
    try{
      websocket = new WebSocket(wsUri);
      websocket.onclose = function () {};
      websocket.onerror = function(evt) {
        ons.notification.toast({message: 'Cannot connect to device', timeout: 2000});
        retry();
      };
      websocket.onopen = function(evt) {
        ons.notification.toast({message: 'Connected to device', timeout: 2000});
        setTimeout(function(){doSend('{"action": "update"}');}, 1000);
      };
      websocket.onclose = function(evt) {
        ons.notification.toast({message: 'Disconnected. Retrying', timeout: 2000});
        retry();
      };
      websocket.onmessage = function(evt) {
        try{
          var msg = JSON.parse(evt.data);
          connectedSteppers.forEach(function(stepperNum) {
            if (typeof msg['position' + stepperNum] !== 'undefined'){
                $('#pbar' + stepperNum).attr('value', msg['position' + stepperNum]);
              };
              if (typeof msg['set' + stepperNum] !== 'undefined'){
                $('#setrange' + stepperNum).val(msg['set' + stepperNum]);
                $('#setrange' + stepperNum).attr('value', msg['position' + stepperNum]);
              };
          });
        } catch(err){}
      };
    } catch (e){
      ons.notification.toast({message: 'Cannot connect to device. Retrying...', timeout: 2000});
      retry();
    };
  };
  function doSend(msg){
    if (websocket && websocket.readyState == 1){
      websocket.send(String(msg));
    }
  };
  function wipeSettings() {
    if (confirm("Are you really sure to wipe ALL settings?")) {
      $.ajax({
        type: "POST",
        url: "/reset",
        contentType : 'application/json',
      })
    }
  };
  window.addEventListener('load', init, false);
  window.onbeforeunload = function() {
    if (websocket && websocket.readyState == 1){
      websocket.close();
    };
  };
  </script>
</head>
<body>

<ons-splitter>
  <ons-splitter-side id='menu' side='left' width='220px' collapse swipeable>
    <ons-page>
      <ons-list>
        <ons-list-item onclick='fn.load("home.html")' tappable>
          Home
        </ons-list-item>
        <ons-list-item onclick='fn.load("settings.html")' tappable>
          Settings
        </ons-list-item>
        <ons-list-item onclick='fn.load("about.html")' tappable>
          About
        </ons-list-item>
        <ons-list-item onclick='wipeSettings()' tappable>
          Wipe settings
        </ons-list-item>
      </ons-list>
    </ons-page>
  </ons-splitter-side>
  <ons-splitter-content id='content' page='home.html'></ons-splitter-content>
</ons-splitter>

<template id='home.html'>
  <ons-page>
    <ons-toolbar>
      <div class='left'>
        <ons-toolbar-button onclick='fn.open()'>
          <ons-icon icon='md-menu'></ons-icon>
        </ons-toolbar-button>
      </div>
      <div class='center'>
        {NAME}
      </div>
    </ons-toolbar>
    <ons-card id="steppers-position">
        <div class='title'>Adjust position</div>
        <div class='content'><p>Move the slider to the wanted position or use the arrows to open/close to the max positions</p></div>
        <!-- Steppers controls will be here -->
    </ons-card>
    <ons-card id='update-card' style='display:none'>
      <div class='title'>Update available</div>
      <div class='content'>You are running <span id='cversion'></span> and <span id='nversion'></span> is the latest. Go to <a href='https://github.com/eg321/esp32-motorized-roller-blinds/releases'>the repo</a> to download</div>
    </ons-card>
  </ons-page>
</template>

<template id='settings.html'>
  <ons-page>
    <ons-toolbar>
      <div class='left'>
        <ons-toolbar-button onclick='fn.open()'>
          <ons-icon icon='md-menu'></ons-icon>
        </ons-toolbar-button>
      </div>
      <div class='center'>
        Settings
      </div>
    </ons-toolbar>
  <ons-card>
    <div class='title'>Instructions</div>
    <div class='content'>
    <p>
    <ol>
      <li>Use the arrows and stop button to navigate to the top position i.e. the blind is opened</li>
      <li>Click the START button</li>
      <li>Use the down arrow to navigate to the max closed position</li>
      <li>Click the MAX button</li>
      <li>Calibration is completed!</li>
    </ol>
    </p>
  </div>
  </ons-card>
  <div id="page-settings">
<!-- Settings controls will be created here -->
  </div>
  </ons-page>
</template>

<template id='about.html'>
  <ons-page>
    <ons-toolbar>
      <div class='left'>
        <ons-toolbar-button onclick='fn.open()'>
          <ons-icon icon='md-menu'></ons-icon>
        </ons-toolbar-button>
      </div>
      <div class='center'>
        About
      </div>
    </ons-toolbar>
  <ons-card>
    <div class='title'>Motor on a roller blind</div>
    <div class='content'>
    <p>
      <ul>
        <li>3d print files and instructions: <a href='https://www.thingiverse.com/thing:4093205' target='_blank'>https://www.thingiverse.com/thing:4093205</a></li>
        <li>Forked from: <a href='https://github.com/nidayand/motor-on-roller-blind-ws' target='_blank'>https://github.com/nidayand/motor-on-roller-blind-ws</a></li>
        <li>Current fork on Github: <a href='https://github.com/eg321/motor-on-roller-blind-ws' target='_blank'>https://github.com/eg321/motor-on-roller-blind-ws</a></li>
        <li>Licensed under <a href='https://raw.githubusercontent.com/nidayand/motor-on-roller-blind-ws/master/LICENSE' target='_blank'>MIT License</a></li>
      </ul>
    </p>
  </div>
  </ons-card>
  </ons-page>
</template>

</body>
</html>
)";
