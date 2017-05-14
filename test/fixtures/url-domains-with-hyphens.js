'use strict';

module.exports = {
  valid: [
    // URLs with hyphen
    {
      ascii: 'r4---sn-a5mlrn7s.gevideo.com',
      unicode: 'r4---sn-a5mlrn7s.gevideo.com'
    },
    {
      ascii: '-sn-a5mlrn7s.gevideo.com',
      unicode: '-sn-a5mlrn7s.gevideo.com'
    },
    {
      ascii: 'sn-a5mlrn7s-.gevideo.com',
      unicode: 'sn-a5mlrn7s-.gevideo.com'
    },
    {
      ascii: '-sn-a5mlrn7s-.gevideo.com',
      unicode: '-sn-a5mlrn7s-.gevideo.com'
    },
    {
      ascii: '-sn--a5mlrn7s-.gevideo.com',
      unicode: '-sn--a5mlrn7s-.gevideo.com'
    }
  ]
}
