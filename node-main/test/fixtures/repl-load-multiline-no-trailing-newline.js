// The lack of a newline at the end of this file is intentional.
const getLunch = () =>
  placeOrder('tacos')
    .then(eat);

const placeOrder = (order) => Promise.resolve(order);
const eat = (food) => '<nom nom nom>';