const axios = require('axios');
const url = 'https://upload.wikimedia.org/wikipedia/commons/c/c7/Mount_Elbrus_May_2008.jpg';

axios.get(url).then((res) => {
  console.log(res.data);
});
