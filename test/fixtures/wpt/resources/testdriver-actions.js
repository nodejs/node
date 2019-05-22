(function() {
  let sourceNameIdx = 0;

  /**
   * Builder for creating a sequence of actions
   */
  function Actions() {
    this.sourceTypes = new Map([["key", KeySource],
                                ["pointer", PointerSource],
                                ["none", GeneralSource]]);
    this.sources = new Map();
    this.sourceOrder = [];
    for (let sourceType of this.sourceTypes.keys()) {
      this.sources.set(sourceType, new Map());
    }
    this.currentSources = new Map();
    for (let sourceType of this.sourceTypes.keys()) {
      this.currentSources.set(sourceType, null);
    }
    this.createSource("none");
    this.tickIdx = 0;
  }

  Actions.prototype = {
    ButtonType: {
      LEFT: 0,
      MIDDLE: 1,
      RIGHT: 2,
      BACK: 3,
      FORWARD: 4,
    },

    /**
     * Generate the action sequence suitable for passing to
     * test_driver.action_sequence
     *
     * @returns {Array} Array of WebDriver-compatible actions sequences
     */
    serialize: function() {
      let actions = [];
      for (let [sourceType, sourceName] of this.sourceOrder) {
        let source = this.sources.get(sourceType).get(sourceName);
        let serialized = source.serialize(this.tickIdx + 1);
        if (serialized) {
          serialized.id = sourceName;
          actions.push(serialized);
        }
      }
      return actions;
    },

    /**
     * Generate and send the action sequence
     *
     * @returns {Promise} fulfilled after the sequence is executed,
     *                    rejected if any actions fail.
     */
    send: function() {
      let actions;
      try {
        actions = this.serialize();
      } catch(e) {
        return Promise.reject(e);
      }
      return test_driver.action_sequence(actions);
    },

    /**
     * Get the action source with a particular source type and name.
     * If no name is passed, a new source with the given type is
     * created.
     *
     * @param {String} type - Source type ('none', 'key', or 'pointer')
     * @param {String?} name - Name of the source
     * @returns {Source} Source object for that source.
     */
    getSource: function(type, name) {
      if (!this.sources.has(type)) {
        throw new Error(`${type} is not a valid action type`);
      }
      if (name === null || name === undefined) {
        name = this.currentSources.get(type);
      }
      if (name === null || name === undefined) {
        return this.createSource(type, null);
      }
      return this.sources.get(type).get(name);
    },

    setSource: function(type, name) {
      if (!this.sources.has(type)) {
        throw new Error(`${type} is not a valid action type`);
      }
      if (!this.sources.get(type).has(name)) {
        throw new Error(`${name} is not a valid source for ${type}`);
      }
      this.currentSources.set(type, name);
      return this;
    },

    /**
     * Add a new key input source with the given name
     *
     * @param {String} name - Name of the key source
     * @param {Bool} set - Set source as the default key source
     * @returns {Actions}
     */
    addKeyboard: function(name, set=true) {
      this.createSource("key", name);
      if (set) {
        this.setKeyboard(name);
      }
      return this;
    },

    /**
     * Set the current default key source
     *
     * @param {String} name - Name of the key source
     * @returns {Actions}
     */
    setKeyboard: function(name) {
      this.setSource("key", name);
      return this;
    },

    /**
     * Add a new pointer input source with the given name
     *
     * @param {String} type - Name of the key source
     * @param {String} pointerType - Type of pointing device
     * @param {Bool} set - Set source as the default key source
     * @returns {Actions}
     */
    addPointer: function(name, pointerType="mouse", set=true) {
      this.createSource("pointer", name, {pointerType: pointerType});
      if (set) {
        this.setPointer(name);
      }
      return this;
    },

    /**
     * Set the current default pointer source
     *
     * @param {String} name - Name of the pointer source
     * @returns {Actions}
     */
    setPointer: function(name) {
      this.setSource("pointer", name);
      return this;
    },

    createSource: function(type, name, parameters={}) {
      if (!this.sources.has(type)) {
        throw new Error(`${type} is not a valid action type`);
      }
      let sourceNames = new Set();
      for (let [_, name] of this.sourceOrder) {
        sourceNames.add(name);
      }
      if (!name) {
        do {
          name = "" + sourceNameIdx++;
        } while (sourceNames.has(name))
      } else {
        if (sourceNames.has(name)) {
          throw new Error(`Alreay have a source of type ${type} named ${name}.`);
        }
      }
      this.sources.get(type).set(name, new (this.sourceTypes.get(type))(parameters));
      this.currentSources.set(type, name);
      this.sourceOrder.push([type, name]);
      return this.sources.get(type).get(name);
    },

    /**
     * Insert a new actions tick
     *
     * @param {Number?} duration - Minimum length of the tick in ms.
     * @returns {Actions}
     */
    addTick: function(duration) {
      this.tickIdx += 1;
      if (duration) {
        this.pause(duration);
      }
      return this;
    },

    /**
     * Add a pause to the current tick
     *
     * @param {Number?} duration - Minimum length of the tick in ms.
     * @returns {Actions}
     */
    pause: function(duration) {
      this.getSource("none").addPause(this, duration);
      return this;
    },

    /**
     * Create a keyDown event for the current default key source
     *
     * @param {String} key - Key to press
     * @param {String?} sourceName - Named key source to use or null for the default key source
     * @returns {Actions}
     */
    keyDown: function(key, {sourceName=null}={}) {
      let source = this.getSource("key", sourceName);
      source.keyDown(this, key);
      return this;
    },

    /**
     * Create a keyDown event for the current default key source
     *
     * @param {String} key - Key to release
     * @param {String?} sourceName - Named key source to use or null for the default key source
     * @returns {Actions}
     */
    keyUp: function(key, {sourceName=null}={}) {
      let source = this.getSource("key", sourceName);
      source.keyUp(this, key);
      return this;
    },

    /**
     * Create a pointerDown event for the current default pointer source
     *
     * @param {String} button - Button to press
     * @param {String?} sourceName - Named pointer source to use or null for the default
     *                               pointer source
     * @returns {Actions}
     */
    pointerDown: function({button=this.ButtonType.LEFT, sourceName=null}={}) {
      let source = this.getSource("pointer", sourceName);
      source.pointerDown(this, button);
      return this;
    },

    /**
     * Create a pointerUp event for the current default pointer source
     *
     * @param {String} button - Button to release
     * @param {String?} sourceName - Named pointer source to use or null for the default pointer
     *                               source
     * @returns {Actions}
     */
    pointerUp: function({button=this.ButtonType.LEFT, sourceName=null}={}) {
      let source = this.getSource("pointer", sourceName);
      source.pointerUp(this, button);
      return this;
    },

    /**
     * Create a move event for the current default pointer source
     *
     * @param {Number} x - Destination x coordinate
     * @param {Number} y - Destination y coordinate
     * @param {String|Element} origin - Origin of the coordinate system.
     *                                  Either "pointer", "viewport" or an Element
     * @param {Number?} duration - Time in ms for the move
     * @param {String?} sourceName - Named pointer source to use or null for the default pointer
     *                               source
     * @returns {Actions}
     */
    pointerMove: function(x, y,
                          {origin="viewport", duration, sourceName=null}={}) {
      let source = this.getSource("pointer", sourceName);
      source.pointerMove(this, x, y, duration, origin);
      return this;
    },
  };

  function GeneralSource() {
    this.actions = new Map();
  }

  GeneralSource.prototype = {
    serialize: function(tickCount) {
      if (!this.actions.size) {
        return undefined;
      }
      let actions = [];
      let data = {"type": "none", "actions": actions};
      for (let i=0; i<tickCount; i++) {
        if (this.actions.has(i)) {
          actions.push(this.actions.get(i));
        } else {
          actions.push({"type": "pause"});
        }
      }
      return data;
    },

    addPause: function(actions, duration) {
      let tick = actions.tickIdx;
      if (this.actions.has(tick)) {
        throw new Error(`Already have a pause action for the current tick`);
      }
      this.actions.set(tick, {type: "pause", duration: duration});
    },
  };

  function KeySource() {
    this.actions = new Map();
  }

  KeySource.prototype = {
    serialize: function(tickCount) {
      if (!this.actions.size) {
        return undefined;
      }
      let actions = [];
      let data = {"type": "key", "actions": actions};
      for (let i=0; i<tickCount; i++) {
        if (this.actions.has(i)) {
          actions.push(this.actions.get(i));
        } else {
          actions.push({"type": "pause"});
        }
      }
      return data;
    },

    keyDown: function(actions, key) {
      let tick = actions.tickIdx;
      if (this.actions.has(tick)) {
        tick = actions.addTick().tickIdx;
      }
      this.actions.set(tick, {type: "keyDown", value: key});
    },

    keyUp: function(actions, key) {
      let tick = actions.tickIdx;
      if (this.actions.has(tick)) {
        tick = actions.addTick().tickIdx;
      }
      this.actions.set(tick, {type: "keyUp", value: key});
    },
  };

  function PointerSource(parameters={pointerType: "mouse"}) {
    let pointerType = parameters.pointerType || "mouse";
    if (!["mouse", "pen", "touch"].includes(pointerType)) {
      throw new Error(`Invalid pointerType ${pointerType}`);
    }
    this.type = pointerType;
    this.actions = new Map();
  }

  PointerSource.prototype = {
    serialize: function(tickCount) {
      if (!this.actions.size) {
        return undefined;
      }
      let actions = [];
      let data = {"type": "pointer", "actions": actions, "parameters": {"pointerType": this.type}};
      for (let i=0; i<tickCount; i++) {
        if (this.actions.has(i)) {
          actions.push(this.actions.get(i));
        } else {
          actions.push({"type": "pause"});
        }
      }
      return data;
    },

    pointerDown: function(actions, button) {
      let tick = actions.tickIdx;
      if (this.actions.has(tick)) {
        tick = actions.addTick().tickIdx;
      }
      this.actions.set(tick, {type: "pointerDown", button});
    },

    pointerUp: function(actions, button) {
      let tick = actions.tickIdx;
      if (this.actions.has(tick)) {
        tick = actions.addTick().tickIdx;
      }
      this.actions.set(tick, {type: "pointerUp", button});
    },

    pointerMove: function(actions, x, y, duration, origin) {
      let tick = actions.tickIdx;
      if (this.actions.has(tick)) {
        tick = actions.addTick().tickIdx;
      }
      this.actions.set(tick, {type: "pointerMove", x, y, origin});
      if (duration) {
        this.actions.get(tick).duration = duration;
      }
    },
  };

  test_driver.Actions = Actions;
})();
