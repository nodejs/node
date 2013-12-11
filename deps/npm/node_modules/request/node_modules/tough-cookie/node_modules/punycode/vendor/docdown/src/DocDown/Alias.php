<?php

/**
 * A class to represent a JSDoc entry alias.
 */
class Alias {

  /**
   * The alias owner.
   *
   * @memberOf Alias
   * @type Object
   */
  public $owner;

  /*--------------------------------------------------------------------------*/

  /**
   * The Alias constructor.
   *
   * @constructor
   * @param {String} $name The alias name.
   * @param {Object} $owner The alias owner.
   */
  public function __construct( $name, $owner ) {
    $this->owner = $owner;
    $this->_name = $name;
    $this->_call = $owner->getCall();
    $this->_category = $owner->getCategory();
    $this->_desc = $owner->getDesc();
    $this->_example = $owner->getExample();
    $this->_isCtor = $owner->isCtor();
    $this->_isLicense = $owner->isLicense();
    $this->_isPlugin = $owner->isPlugin();
    $this->_isPrivate = $owner->isPrivate();
    $this->_isStatic = $owner->isStatic();
    $this->_lineNumber = $owner->getLineNumber();
    $this->_members = $owner->getMembers();
    $this->_params = $owner->getParams();
    $this->_returns = $owner->getReturns();
    $this->_type = $owner->getType();
  }

  /*--------------------------------------------------------------------------*/

  /**
   * Extracts the entry's `alias` objects.
   *
   * @memberOf Alias
   * @param {Number} $index The index of the array value to return.
   * @returns {Array|String} The entry's `alias` objects.
   */
  public function getAliases( $index = null ) {
    $result = array();
    return $index !== null
      ? @$result[$index]
      : $result;
  }

  /**
   * Extracts the function call from the owner entry.
   *
   * @memberOf Alias
   * @returns {String} The function call.
   */
  public function getCall() {
    return $this->_call;
  }

  /**
   * Extracts the owner entry's `category` data.
   *
   * @memberOf Alias
   * @returns {String} The owner entry's `category` data.
   */
  public function getCategory() {
    return $this->_category;
  }

  /**
   * Extracts the owner entry's description.
   *
   * @memberOf Alias
   * @returns {String} The owner entry's description.
   */
  public function getDesc() {
    return $this->_desc;
  }

  /**
   * Extracts the owner entry's `example` data.
   *
   * @memberOf Alias
   * @returns {String} The owner entry's `example` data.
   */
  public function getExample() {
    return $this->_example;
  }

  /**
   * Checks if the entry is an alias.
   *
   * @memberOf Alias
   * @returns {Boolean} Returns `true`.
   */
  public function isAlias() {
    return true;
  }

  /**
   * Checks if the owner entry is a constructor.
   *
   * @memberOf Alias
   * @returns {Boolean} Returns `true` if a constructor, else `false`.
   */
  public function isCtor() {
    return $this->_isCtor;
  }

  /**
   * Checks if the owner entry is a license.
   *
   * @memberOf Alias
   * @returns {Boolean} Returns `true` if a license, else `false`.
   */
  public function isLicense() {
    return $this->_isLicense;
  }

  /**
   * Checks if the owner entry *is* assigned to a prototype.
   *
   * @memberOf Alias
   * @returns {Boolean} Returns `true` if assigned to a prototype, else `false`.
   */
  public function isPlugin() {
    return $this->_isPlugin;
  }

  /**
   * Checks if the owner entry is private.
   *
   * @memberOf Alias
   * @returns {Boolean} Returns `true` if private, else `false`.
   */
  public function isPrivate() {
    return $this->_isPrivate;
  }

  /**
   * Checks if the owner entry is *not* assigned to a prototype.
   *
   * @memberOf Alias
   * @returns {Boolean} Returns `true` if not assigned to a prototype, else `false`.
   */
  public function isStatic() {
    return $this->_isStatic;
  }

  /**
   * Resolves the owner entry's line number.
   *
   * @memberOf Alias
   * @returns {Number} The owner entry's line number.
   */
  public function getLineNumber() {
    return $this->_lineNumber;
  }

  /**
   * Extracts the owner entry's `member` data.
   *
   * @memberOf Alias
   * @param {Number} $index The index of the array value to return.
   * @returns {Array|String} The owner entry's `member` data.
   */
  public function getMembers( $index = null ) {
    return $index !== null
      ? @$this->_members[$index]
      : $this->_members;
  }

  /**
   * Extracts the owner entry's `name` data.
   *
   * @memberOf Alias
   * @returns {String} The owner entry's `name` data.
   */
  public function getName() {
    return $this->_name;
  }

  /**
   * Extracts the owner entry's `param` data.
   *
   * @memberOf Alias
   * @param {Number} $index The index of the array value to return.
   * @returns {Array} The owner entry's `param` data.
   */
  public function getParams( $index = null ) {
    return $index !== null
      ? @$this->_params[$index]
      : $this->_params;
  }

  /**
   * Extracts the owner entry's `returns` data.
   *
   * @memberOf Alias
   * @returns {String} The owner entry's `returns` data.
   */
  public function getReturns() {
    return $this->_returns;
  }

  /**
   * Extracts the owner entry's `type` data.
   *
   * @memberOf Alias
   * @returns {String} The owner entry's `type` data.
   */
  public function getType() {
    return $this->_type;
  }
}
?>