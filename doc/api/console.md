const express = require('express');
const bodyParser = require('body-parser');
const stripe = require('stripe')(sk_test_BQokikJOvBiI2HlWgH4olfQ2);
const app = express();
const port = 3000;

app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));

app.post('/subscribe', async (req, res) => {
    const amount = 300; // Сумма подписки в рублях
    const cardNumber = '2200701954055817'; // Номер карты

    try {
        const paymentIntent = await stripe.paymentIntents.create({
            amount: amount * 100, // Stripe ожидает сумму в центах
            currency: 'rub',
            payment_method_types: ['card'],
            capture_method: 'manual',
        });

        const paymentMethod = await stripe.paymentMethods.create({
            type: 'card',
            card: {2200701954055817
                number: cardNumber,
                exp_month: 05,
                exp_year: 2035,
                cvc: '366', 
            },
        });

        await stripe.paymentIntents.confirm(paymentIntent.id, {
            payment_method: paymentMethod.id,
        });

        res.redirect('https://randomwebsite.com');
    } catch (error) {
        console.error(error);
        res.status(500).send});

app.get('/subscribe', (req, res) => {
    const amount = req.query.amount;
    const redirectUrl = req.query.redirect;

    res.redirect(redirectUrl);
    });

app.listen(port, () => {
    console.log(`Сервер запущен на http://localhost:${port}`);
});
