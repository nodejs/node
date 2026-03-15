import { test } from 'node:test';

test('worker ID is available as environment variable', (t) => {
	const workerId = process.env.NODE_TEST_WORKER_ID;
	if (workerId === undefined) {
		throw new Error('NODE_TEST_WORKER_ID should be defined');
	}

	const id = Number(workerId);
	if (isNaN(id) || id < 1) {
		throw new Error(`Invalid worker ID: ${workerId}`);
	}
});

test('worker ID is available via context', (t) => {
	const workerId = t.workerId;
	const envWorkerId = process.env.NODE_TEST_WORKER_ID;

	if (workerId === undefined) {
		throw new Error('context.workerId should be defined');
	}

	if (workerId !== Number(envWorkerId)) {
		throw new Error(`context.workerId (${workerId}) should match NODE_TEST_WORKER_ID (${envWorkerId})`);
	}
});
